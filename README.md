# Simulador de Memoria Virtual (paginación de 2 niveles)

Laboratorio de Sistemas Operativos. Simula la traducción VA → PA con una tabla
de páginas de dos niveles, fallos de página con *demand paging*, un área de swap
y reemplazo de páginas **LRU**.

**Integrantes:** _Jhon Alejandro García Pareja, Sara Galván Ortega, David Tovar Zurita_
**Política de reemplazo:** LRU.

## Compilación

Requisitos: `gcc` y `make`.

```bash
make            # compila ./vmsim  (gcc -Wall -Wextra -Werror -std=c99)
make clean      # borra ./vmsim y build/
```

## Uso

```bash
./vmsim [opciones] [archivo]
```

| Opción | Descripción | Por defecto |
|---|---|---|
| `-m <KB>` | memoria física en KB (mínimo 256) | `256` |
| `-s <bytes>` | tamaño de página (potencia de 2, 256..65536) | `4096` |
| `-v` | muestra los campos de VA, su traducción binaria a PA y la tabla final | apagado |
| `-q` | no imprime cada operación, solo las estadísticas | apagado |

Si no se da archivo, lee de la entrada estándar.

Atajos con Make:

```bash
make run                                              # ejemplo del enunciado con LRU
make run INPUT=tests/test3_localidad.txt              # elegir archivo
make run MEM_KB=512 INPUT=tests/test4_aleatorio.txt
make test                                             # verifica integridad de datos con LRU
make compare                                          # corre todas las pruebas con LRU
make valgrind                                         # leak-check con valgrind
```

## Formato de entrada

Tokens separados por espacios o saltos de línea (funcionan ambos: una operación
por línea, o todo en una sola línea como en el enunciado). `#` inicia un comentario.

```
alloc <bytes>              reserva memoria virtual; la primera alloc empieza en VA 0
write <virtual_addr> <v>   escribe el byte v en la dirección
read  <virtual_addr>       lee el byte de la dirección
free  <virtual_addr>       libera la región que EMPIEZA en esa dirección
dump                       imprime la tabla de páginas completa (ver abajo)
```

Las direcciones pueden ser decimales o hexadecimales (`0x1000`).

Ejemplo:

```
alloc 8192
write 0 42
write 4096 99
read 0
read 4096
```

## Ver la traducción VA → PA y la tabla de páginas

Con `-v` (verboso), primero muestra cómo se divide la VA. Con los valores por
defecto son 10 bits para PT1, 10 para PT2 y 12 para el offset. Después imprime
cada dirección en binario agrupada por campo, los índices decimales y la PA en
decimal y binario. Al terminar muestra la tabla de páginas final.

```
$ ./vmsim -v ejemplo.txt
========== Formato de dirección virtual ==========
VA (32 bits) = [ PT1: 10 bits ] [ PT2: 10 bits ] [ Offset: 12 bits ]
PT1: bits [31:22], índice de la tabla de nivel 1
PT2: bits [21:12], índice de la tabla de nivel 2
Offset: bits [11:0], posición dentro de la página
...
    Traducción VA -> PA:
      VA decimal: 4096
      VA binaria: 0000000000 | 0000000001 | 000000000000
      PT1 (bits [31:22]) = 0 -> entrada de tabla de nivel 1
      PT2 (bits [21:12]) = 1 -> entrada de tabla de nivel 2
      Offset (bits [11:0]) = 0 -> byte dentro de la página
      VPN 1 -> PFN/frame 1 (página cargada tras fallo)
      PA decimal: 4096, binaria: 00000000000000000001000000000000
...
----- Tabla de páginas (PT1.PT2 = VPN) -----
PT1: bits [31:22] | PT2: bits [21:12] | Offset: bits [11:0]
```

El comando `dump` también permite imprimir la tabla en cualquier punto del
archivo de entrada. Se muestran las páginas tocadas: `valid=1` o con copia en
swap.

```
alloc 8192
write 0 42
write 4096 99
dump
```

```
----- Tabla de páginas (PT1.PT2 = VPN) -----
PT1    PT2    VPN        valid    acc    dirty  frame  swap
0      0      0          1        1      1      0      -
0      1      1          1        1      1      1      -
---------------------------------------------
```

Si una página fue expulsada de RAM (reemplazo), su fila muestra `valid=0` y,
si estaba modificada, el número de su slot en `swap` en vez de `frame`.

## Estructura del repositorio

```
lab-memoria-virtual/
├── Makefile
├── README.md
├── REPORTE.md               reporte de análisis
├── include/
│   ├── cli.h                opciones de línea de comandos
│   ├── commands.h           ejecución del lenguaje de comandos
│   ├── config.h             constantes, política, vm_config_t (bits de la VA)
│   ├── parse.h               conversión compartida de enteros
│   ├── pagetable.h          PTE y tabla de 2 niveles
│   ├── physmem.h            memoria física (frames) + swap
│   ├── replacement.h        interfaz de LRU
│   └── vmm.h                estructura principal, estadísticas, API
├── src/
│   ├── main.c                orquestación y ciclo de vida
│   ├── cli/cli.c             análisis de opciones
│   ├── commands/commands.c   parser y despacho de operaciones (Command)
│   ├── common/parse.c        conversión compartida de enteros
│   ├── config/config.c       validación de parámetros
│   ├── memory/
│   │   ├── pagetable.c       índices PT1/PT2/offset y tablas L2
│   │   └── physmem.c         frames libres y slots de swap
│   ├── replacement/replacement.c  política LRU
│   └── vmm/
│       ├── pagefault.c       fallos de página y expulsión de víctima
│       ├── translate.c       traducción VA -> PA
│       └── vmm.c             alloc / free / read / write / estadísticas
└── tests/
    ├── gen_tests.sh         regenera los tests 2 a 5
    ├── test1_basico.txt     ejemplo del enunciado
    ├── test2_secuencial.txt recorrido cíclico mayor que la RAM
    ├── test3_localidad.txt  conjunto caliente + streaming
    ├── test4_aleatorio.txt  5000 accesos aleatorios con localidad
    └── test5_integridad.*   verifica que los datos sobreviven al swap
```

## Decisiones de diseño

- **Responsabilidad única:** `main.c` coordina el ciclo de vida; `cli.c` analiza
  opciones; `commands.c` interpreta y ejecuta las operaciones; `parse.c` comparte
  la conversión de números.
- **Command:** cada operación (`alloc`, `write`, `read`, `free`, `dump`) tiene un
  manejador registrado en una tabla. Así se mantiene separado el despacho de la
  lógica propia de cada comando.

- **Demand paging:** `alloc` solo reserva direcciones virtuales; una página se trae a
  RAM en su primer acceso (cuenta como fallo). Ahí se crea la tabla L2 si no existe.
- **Memoria real simulada:** la RAM es un arreglo de bytes; `write`/`read` guardan y
  devuelven datos reales. Cada dirección guarda **1 byte** (valores > 255 se truncan
  con aviso).
- **Swap:** al expulsar una página *sucia* (`dirty`) se copia a un slot de swap; si se
  vuelve a usar, se recupera desde ahí. Las páginas limpias se descartan.
- **Accesos inválidos:** una dirección fuera de las regiones asignadas imprime un
  error (segfault simulado), no cuenta en «Total de accesos» y la ejecución sigue.
- **Fallo de página:** se cuenta un fallo por acceso que no encuentra la página en RAM;
  el reintento posterior a cargarla es un *hit*. Hit rate = (N − M) / N × 100.
- **Tiempo simulado:** 100 ns por acceso a RAM, 2 µs por fallo y 5 ms por cada
  lectura/escritura de disco. Solo sirve para comparar políticas.
- **Tamaño de página configurable:** los bits restantes de la VA se reparten entre PT1 y
  PT2 (con 4 KB: 10 + 10 + 12).

## Limitaciones conocidas

- Las direcciones virtuales no se reutilizan tras un `free` (asignación incremental).
- `free` solo acepta la dirección inicial de una región, no una dirección intermedia.
- No hay TLB (no lo pide el enunciado).
