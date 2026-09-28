# Reporte de Análisis — Simulador de Memoria Virtual

**Integrantes:** _Jhon Alejandro García Pareja, Sara Galván Ortega, David Tovar Zurita_
**Política implementada actualmente:** LRU
**Configuración base de las pruebas:** página de 4 KB, memoria física de 256 KB (64 frames), espacio virtual de 32 bits.

> Este reporte conserva la comparación histórica entre FIFO y LRU. La versión
> actual del simulador implementa únicamente LRU; sus resultados se reproducen
> con `make compare` o `./vmsim -q -m <KB> tests/<archivo>`.

---

## 1. Descripción de las estructuras de datos

### 1.1 Dirección virtual y tabla de páginas de 2 niveles

```
 31            22 21            12 11               0
┌────────────────┬────────────────┬──────────────────┐
│   PT1 (10 b)   │   PT2 (10 b)   │   Offset (12 b)  │
└───────┬────────┴───────┬────────┴──────────────────┘
        │                │
        ▼                ▼
   Tabla L1          Tabla L2 (se crea bajo demanda)
   1024 punteros ──► 1024 PTEs ──► frame ──► PA = (frame << 12) | offset
   (NULL = L2 no existe)
```

- **`pte_t`** (`pagetable.h`): `frame` (# página física), bits `valid`, `accessed`, `dirty`
  y `swap_slot` (índice de la copia en disco, −1 si no existe).
- **`pagetable_t`**: arreglo L1 de `2^pt1_bits` punteros a arreglos L2 de PTEs. Una tabla
  L2 solo se crea (`calloc`) cuando ocurre el primer fallo en su rango y se libera
  en `free` cuando queda sin PTEs en uso. Esto ahorra memoria: un programa que usa pocas
  regiones de su espacio de 4 GB solo paga por las tablas L2 que realmente toca
  (cada una es de 12 KB en este simulador).
- El tamaño de página es configurable (`-s`); los bits restantes se reparten entre PT1 y PT2.

### 1.2 Memoria física y swap (`physmem_t`)

- `mem`: arreglo real de bytes de `num_frames × page_size`. Guarda los datos de `write`/`read`.
- `frames[]`: tabla inversa (¿qué página virtual vive en cada frame?). Se necesita para
  saber qué PTE invalidar cuando se expulsa un frame.
- Pila de frames libres (el primero en entregarse es el 0, para que la salida sea determinista).
- **Swap simulado:** slots de una página, creados bajo demanda y reutilizables. Sin esto,
  una página modificada que se expulsa perdería sus datos.

### 1.3 Regiones (`region_t`)

`alloc` reserva un rango virtual `[base, base+size)` contiguo y alineado a página. Cualquier acceso
fuera de las regiones vivas se rechaza como acceso inválido.

### 1.4 Estructura de la política de reemplazo (`replacer_t`)

Una **lista doblemente enlazada de frames** implementada con arreglos `prev[]`/`next[]`
indexados por número de frame, con `head` (candidato a víctima) y `tail` (más nuevo).
Todas las operaciones son O(1): insertar, mover, sacar y elegir víctima.

---

## 2. Explicación de las políticas de reemplazo

La implementación actual utiliza solo LRU. La descripción de FIFO y las tablas
comparativas siguientes se conservan como análisis académico de ambas políticas.

Ambas políticas comparten la lista; solo cambia la reacción ante un *hit* (`repl_on_access`):

| Evento | FIFO | LRU |
|---|---|---|
| Página se carga en un frame | se agrega al final (`tail`) | se agrega al final (`tail`) |
| Hit sobre una página en RAM | **no hace nada** | la mueve al final (`tail`) |
| Elegir víctima | `head` = la que **llegó** hace más tiempo | `head` = la que **se usó** hace más tiempo |

**FIFO** solo considera el orden de llegada: es simple y determinista, pero puede expulsar una
página muy usada solo porque es antigua. **LRU** usa la localidad temporal: si una página
se usó hace poco, es probable que se use pronto, así que se expulsa la que lleva más tiempo
sin usarse.

**Flujo de un fallo (`pagefault.c`):** contar el fallo → crear tabla L2 si falta → pedir un frame libre →
si no hay, elegir víctima con la política; si la víctima está `dirty`, escribirla al swap → cargar la
página (desde swap si ya existía, con ceros si es la primera vez) → `valid = 1` → avisar a la política.

---

## 3. Programas de prueba y resultados

Configuración: 256 KB de RAM (64 frames de 4 KB).

### 3.1 Resumen

| Prueba | Política | Accesos | Fallos | Hit rate | Reemplazos | Write-backs | Tiempo simulado |
|---|---|---:|---:|---:|---:|---:|---:|
| test1 (ejemplo) | FIFO | 6 | 2 | 66.67 % | 0 | 0 | 0.005 ms |
| test1 (ejemplo) | LRU | 6 | 2 | 66.67 % | 0 | 0 | 0.005 ms |
| test2 secuencial | FIFO | 240 | 240 | 0.00 % | 176 | 0 | 0.504 ms |
| test2 secuencial | LRU | 240 | 240 | 0.00 % | 176 | 0 | 0.504 ms |
| test3 localidad | FIFO | 7392 | 448 | 93.94 % | 384 | 0 | 1.635 ms |
| test3 localidad | LRU | 7392 | 256 | 96.54 % | 192 | 0 | 1.251 ms |
| test4 aleatorio | FIFO | 5000 | 2035 | 59.30 % | 1971 | 1014 | 12594.6 ms |
| test4 aleatorio | LRU | 5000 | 1591 | 68.18 % | 1527 | 739 | 8968.7 ms |

### 3.2 Descripción de cada prueba

- **test1 — ejemplo del enunciado.** 2 páginas; de 6 accesos, 2 son fallos
  *obligatorios* (primer acceso a cada página) y 4 son hits. Incluye un acceso inválido (`read 8192`) que se reporta
  como segfault simulado sin afectar las estadísticas.
- **test2 — recorrido secuencial cíclico.** 80 páginas (320 KB) leídas en 3 pasadas con solo 64 frames.
- **test3 — localidad.** 32 páginas «calientes» (128 KB) leídas antes de cada una de 224 páginas de
  un flujo que se toca una sola vez (1 MB en total).
- **test4 — aleatorio con localidad.** 5000 accesos sobre 1 MB: 80 % van al ~20 % de las páginas;
  30 % son escrituras (genera páginas sucias y por tanto escrituras a disco).
- **test5 — integridad.** Escribe 100 páginas, las lee en orden inverso y directo. Con 64 frames obliga a
  usar el swap; `make test` comprueba que cada lectura devuelve lo escrito con LRU.

### 3.3 Variación con el tamaño de la memoria física

Hit rate (y reemplazos):

| RAM | test3 FIFO | test3 LRU | test4 FIFO | test4 LRU |
|---:|---:|---:|---:|---:|
| 256 KB | 93.94 % (384) | 96.54 % (192) | 59.30 % (1971) | 68.18 % (1527) |
| 320 KB | 94.81 % (304) | 96.54 % (176) | 67.22 % (1559) | 77.60 % (1040) |
| 384 KB | 95.24 % (256) | 96.54 % (160) | 71.98 % (1305) | 81.74 % (817) |
| 512 KB | 95.67 % (192) | 96.54 % (128) | 79.96 % (874) | 85.74 % (585) |
| 768 KB | 96.10 % (96) | 96.54 % (64) | 89.64 % (326) | 91.50 % (233) |
| 1024 KB | 96.54 % (0) | 96.54 % (0) | 94.92 % (0) | 94.92 % (0) |

Para test2: con 256 KB ambas políticas dan 0 % de hit rate; con 320 KB o más (la región completa
cabe en RAM), ambas suben a 66.67 % (80 fallos obligatorios de 240 accesos) y 0 reemplazos.

---

## 4. Análisis de resultados

**Fallos obligatorios vs. de capacidad.** Cuando la RAM es de 1024 KB, ambas políticas coinciden
(test3: 256 fallos = 256 páginas distintas; test4: 254): esos son los fallos *obligatorios* y ninguna política
puede evitarlos. Toda la diferencia entre políticas está en los fallos de *capacidad*, que aparecen
cuando el conjunto de trabajo no cabe en memoria.

**test2 (secuencial cíclico): empate en el peor caso.** El programa recorre 80 páginas en ciclo con 64 frames.
Con ambas políticas la página que se necesita justo después es la que se acaba de expulsar (la más
antigua y también la menos recientemente usada), así que **cada acceso es un fallo** (0 % de hit rate).
Es el peor caso conocido de LRU y FIFO; aumentar la RAM apenas lo suficiente (320 KB) lo elimina.

**test3 (localidad): LRU gana claramente.** LRU obtiene 256 fallos, el mínimo posible (solo los obligatorios), porque
las 32 páginas calientes se tocan constantemente y nunca son las menos usadas. FIFO obtiene 448: cada
vez que una página caliente pasa a ser la más antigua de la memoria se expulsa aunque se esté usando, y se
paga un fallo extra (192 fallos evitables, 6 rondas × 32 páginas). Con LRU el número de reemplazos baja a la mitad
(384 → 192) y el hit rate sube de 93.94 % a 96.54 %. **FIFO no distingue una página vieja de una página vieja
pero muy usada.**

**test4 (aleatorio con localidad): LRU mejora en todos los tamaños de RAM.** Con 256 KB, LRU reduce los fallos
un 21.8 % (2035 → 1591), los reemplazos un 22.5 %, y las escrituras a disco de 1014 a 739. Eso se
traduce en un tiempo simulado de 8.97 s frente a 12.59 s (−28.8 %), dominado por el costo del disco. La ventaja
se mantiene hasta que la RAM alcanza el tamaño de la región, donde ya no hay reemplazos.

**Efecto del tamaño de la memoria.** El hit rate crece con la RAM en todas las curvas y las diferencias entre políticas
se estrechan; con memoria abundante la política deja de importar. La mayor brecha FIFO-LRU en
test4 aparece con RAM ajustada (320 KB: 67.22 % contra 77.60 %), justo donde la política decide qué se conserva.

**Costo del write-back.** Una página expulsada limpia se descarta; una sucia cuesta una escritura a disco (5 ms simulados). Por eso en
test4 el tiempo depende más de los write-backs y las lecturas de swap que de los accesos a RAM (0.5 ms en total).

---

## 5. Comparación teórica FIFO vs. LRU

| Aspecto | FIFO | LRU |
|---|---|---|
| Criterio | orden de llegada | recencia de uso |
| Aprovecha localidad temporal | No | Sí |
| Anomalía de Belady | **Puede ocurrir**: más frames pueden dar más fallos | **No**: es un algoritmo de pila (*stack algorithm*) |
| Costo por acceso | ninguno (solo se actúa al cargar) | actualizar el orden en **cada** acceso |
| Implementación real | trivial (cola circular) | costosa en hardware; se aproxima con el bit `accessed` (Clock, segunda oportunidad) |
| Peor caso | ciclos poco mayores que la RAM | ciclos poco mayores que la RAM (igual) |

- **No hay política «siempre mejor».** En la secuencia de páginas `0 1 2 3 0 4 1` con 4 frames (se puede comprobar
  con `-s 65536` y las direcciones 0, 65536, 131072, 196608, 0, 262144, 65536), FIFO produce 5 fallos y LRU 6: tras el acceso a `0`
  (hit), LRU deja de considerar «vieja» a la página 0 y expulsa a la 1, que se pide justo después. LRU gana
  cuando hay localidad, no en todos los casos.
- **Óptimo de Belady (OPT):** expulsa la página cuyo próximo uso está más lejos; es una cota inferior
  imposible de implementar (requiere conocer el futuro) y sirve como referencia. LRU es una aproximación de OPT
  suponiendo que el pasado reciente predice el futuro.
- **Costo real de LRU:** este simulador lo hace en O(1) con una lista, pero un sistema operativo real no
  puede reordenar una lista en cada acceso a memoria. Por eso usa aproximaciones basadas en el bit `accessed`
  de la PTE (Clock/NRU), que el hardware sí actualiza gratis.

---

## 6. Conclusiones

1. La traducción de dos niveles con creación bajo demanda de tablas L2 funciona: el simulador
   solo asigna tablas para las zonas del espacio virtual que se usan y las libera con `free`.
2. Sin localidad de referencia (test2), la política es irrelevante; con localidad (test3, test4), los
  resultados históricos muestran que LRU reduce fallos, reemplazos y escrituras a disco frente a FIFO.
3. Los reemplazos de páginas sucias son el costo dominante, por lo que reducir fallos de página
   tiene mucho más impacto que optimizar cualquier otra parte del camino de acceso.
4. Las pruebas de integridad confirman que los datos sobreviven al swap con LRU y con distintos
  tamaños de página (1 KB, 4 KB, 8 KB).
