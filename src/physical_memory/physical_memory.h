#ifndef PHYSICAL_MEMORY_H
#define PHYSICAL_MEMORY_H
#include "sim_virtual.h"

void physical_memory_init(PhysicalMemory *memory, uint32_t size);
void physical_memory_destroy(PhysicalMemory *memory);
uint8_t physical_memory_read_byte(const PhysicalMemory *memory, uint32_t physical_address);
void physical_memory_write_byte(PhysicalMemory *memory, uint32_t physical_address, uint8_t value);
void physical_memory_clear_range(PhysicalMemory *memory, uint32_t start_address, uint32_t length);


#endif