# Makefile - Simulador de memoria virtual con paginación de 2 niveles
#
# Targets principales:
#   make            compila el ejecutable ./vmsim
#   make run        ejecuta un archivo de prueba (ver variables abajo)
#   make clean      borra binarios y objetos
# Extras:
#   make test       verifica la integridad de los datos con LRU
#   make compare    muestra métricas de todas las pruebas con LRU
#   make valgrind   ejecuta valgrind con leak-check completo

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c99 -g -O2 -Iinclude -MMD -MP
TARGET  = vmsim
SRC_DIR = src
OBJ_DIR = build

SRCS = $(shell find $(SRC_DIR) -type f -name '*.c')
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

INPUT  ?= tests/test1_basico.txt
MEM_KB ?= 256
TESTS   = tests/test1_basico.txt tests/test2_secuencial.txt \
          tests/test3_localidad.txt tests/test4_aleatorio.txt

.PHONY: all clean run test compare valgrind

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET) -m $(MEM_KB) $(INPUT)

# Las lecturas de tests/test5 deben devolver exactamente lo que se escribió,
# aunque las páginas hayan pasado por el swap.
test: $(TARGET)
	@./$(TARGET) tests/test5_integridad.txt | grep '^\[read' \
	  | diff -q - tests/test5_integridad.expected > /dev/null \
	  && echo "[OK]   integridad de datos con LRU" \
	  || { echo "[FALLA] integridad de datos con LRU"; exit 1; }

compare: $(TARGET)
	@for t in $(TESTS); do \
	  echo "=== $$t  [LRU] ==="; \
	  ./$(TARGET) -q -m $(MEM_KB) $$t | grep -E 'Total|Hit'; \
	done

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
	  ./$(TARGET) -q $(INPUT)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

-include $(DEPS)
