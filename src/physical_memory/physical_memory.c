#include "sim_virtual.h"
#include <stdio.h>
#include <stdlib.h>

void physical_memory_init(PhysicalMemory *memory, uint32_t size) {
    if (memory == NULL) {
        return;
    }
    memory->size = size;
    memory->storage = (uint8_t *)calloc(size, sizeof(uint8_t));
}

void physical_memory_destroy(PhysicalMemory *memory) {
    if (memory == NULL) {
        return;
    }
    free(memory->storage);
    memory->storage = NULL;
    memory->size = 0u;
}

uint8_t physical_memory_read_byte(const PhysicalMemory *memory, uint32_t physical_address) {
    if (memory == NULL || physical_address >= memory->size) {
        fprintf(stderr, "Direccion fisica fuera de rango\n");
        exit(1);
    }
    return memory->storage[physical_address];
}

void physical_memory_write_byte(PhysicalMemory *memory, uint32_t physical_address, uint8_t value) {
    if (memory == NULL || physical_address >= memory->size) {
        fprintf(stderr, "Direccion fisica fuera de rango\n");
        exit(1);
    }
    memory->storage[physical_address] = value;
}

void physical_memory_clear_range(PhysicalMemory *memory, uint32_t start_address, uint32_t length) {
    if (memory == NULL) {
        return;
    }
    for (uint32_t i = 0; i < length; ++i) {
        if (start_address + i < memory->size) {
            memory->storage[start_address + i] = 0u;
        }
    }
}


