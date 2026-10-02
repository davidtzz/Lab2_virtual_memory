#ifndef MEMORY_CONFIG_H
#define MEMORY_CONFIG_H

#include "sim_virtual.h"

MemoryConfig memory_config_create(uint32_t page_size, uint32_t physical_memory_size); 
bool memory_config_is_valid(uint32_t page_size, uint32_t physical_memory_size);
uint32_t memory_config_compute_offset_bits(uint32_t page_size);
uint32_t memory_config_compute_directory_index_bits(uint32_t vpn_bits);

#endif // MEMORY_CONFIG_H   