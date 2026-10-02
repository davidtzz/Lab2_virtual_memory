#ifndef VIRTUAL_MEMORY_MANAGER_H

#define VIRTUAL_MEMORY_MANAGER_H

#include "sim_virtual.h" 

MemoryManager memory_manager_create(const MemoryConfig *config, SimPolicy policy);
void memory_manager_destroy(MemoryManager *manager);
uint32_t memory_manager_allocate(MemoryManager *manager, uint32_t bytes);
void memory_manager_write(MemoryManager *manager, uint32_t virtual_address, uint8_t value);
uint8_t memory_manager_read(MemoryManager *manager, uint32_t virtual_address);
void memory_manager_free(MemoryManager *manager, uint32_t virtual_address);

#endif 