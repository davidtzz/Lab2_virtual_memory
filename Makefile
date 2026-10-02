CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -Iinclude
LDFLAGS = -lm
TARGET = sim_virtual
SRC = $(shell find src -type f -name "*.c" | sort)
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
