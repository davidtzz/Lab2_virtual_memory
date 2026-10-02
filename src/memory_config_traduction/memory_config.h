#ifndef MEMORY_CONFIG_H
#define MEMORY_CONFIG_H

#include <stdint.h>

MemoryConfig memory_config_create(MemoryConfig *config, uint32_t page_size, uint32_t page_table_entry_count, uint32_t directory_entry_count);
bool memory_config_is_valid(const MemoryConfig *config);
uint32_t memory_config_compute_offset_bits(const MemoryConfig *config);
uint32_t memory_config_compute_directory_index_bits(const MemoryConfig *config);

#endif // MEMORY_CONFIG_H   