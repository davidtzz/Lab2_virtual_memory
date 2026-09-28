#!/bin/sh
# Regenera los archivos de prueba test2..test5 (usa awk).
# Nota: rand() de awk varía entre implementaciones (mawk/gawk); por eso los
# archivos generados ya vienen incluidos en el repositorio.
cd "$(dirname "$0")" || exit 1

# test2: recorrido cíclico de 80 páginas (320 KB) con solo 64 frames (256 KB)
awk 'BEGIN{
  print "# Recorrido secuencial ciclico de 80 paginas (320 KB), 3 pasadas";
  print "alloc 327680";
  for (pass = 0; pass < 3; pass++)
    for (p = 0; p < 80; p++) printf "read %d\n", p * 4096;
}' > test2_secuencial.txt

# test3: 32 paginas "calientes" que se reusan + un stream de 224 paginas
awk 'BEGIN{
  print "# 32 paginas calientes reutilizadas + streaming de 224 paginas (1 MB total)";
  print "alloc 1048576";
  for (s = 32; s < 256; s++) {
    for (h = 0; h < 32; h++) printf "read %d\n", h * 4096;
    printf "read %d\n", s * 4096;
  }
}' > test3_localidad.txt

# test4: 5000 accesos aleatorios, 80% a las primeras 51 paginas, 30% escrituras
awk 'BEGIN{
  srand(42);
  print "# 5000 accesos aleatorios: 80% a ~20% de las paginas, 30% escrituras";
  print "alloc 1048576";
  for (i = 0; i < 5000; i++) {
    if (rand() < 0.8) p = int(rand() * 51); else p = int(51 + rand() * 205);
    addr = p * 4096 + int(rand() * 4096);
    if (rand() < 0.3) printf "write %d %d\n", addr, int(rand() * 256);
    else              printf "read %d\n", addr;
  }
  print "free 0";
}' > test4_aleatorio.txt

# test5: integridad de datos a traves del swap (100 paginas > 64 frames)
awk 'BEGIN{
  print "# Integridad: 100 paginas escritas y releidas (fuerza swap con 256 KB)";
  print "alloc 409600";
  for (p = 0; p < 100; p++) printf "write %d %d\n", p * 4096 + 7, (p * 7) % 251;
  for (p = 99; p >= 0; p--) printf "read %d\n", p * 4096 + 7;
  for (p = 0; p < 100; p++) printf "read %d\n", p * 4096 + 7;
}' > test5_integridad.txt
awk 'BEGIN{
  for (p = 99; p >= 0; p--) printf "[read ] VA 0x%08x -> %d\n", p * 4096 + 7, (p * 7) % 251;
  for (p = 0; p < 100; p++) printf "[read ] VA 0x%08x -> %d\n", p * 4096 + 7, (p * 7) % 251;
}' > test5_integridad.expected
