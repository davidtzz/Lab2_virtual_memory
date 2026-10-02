CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -Iinclude
LDFLAGS = -lm
TARGET = sim_virtual
SRC = src/main.c src/sim_virtual.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
