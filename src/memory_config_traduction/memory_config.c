
#include "sim_virtual.h"
#include <stdio.h>

MemoryConfig memory_config_create(uint32_t page_size, uint32_t physical_memory_size) {
    MemoryConfig config = {0};
    if (!memory_config_is_valid(page_size, physical_memory_size)) {
        fprintf(stderr, "Configuracion invalida: page_size=%u physical_memory_size=%u\n",
                page_size, physical_memory_size);
        exit(1);
    }

    config.page_size = page_size;
    config.physical_memory_size = physical_memory_size;
    config.frame_count = physical_memory_size / page_size;
    config.offset_bits = memory_config_compute_offset_bits(page_size);
    const uint32_t vpn_bits = SIM_VIRTUAL_ADDRESS_BITS - config.offset_bits;
    config.directory_index_bits = memory_config_compute_directory_index_bits(vpn_bits);
    config.page_table_index_bits = vpn_bits - config.directory_index_bits;
    config.directory_entry_count = (uint32_t)pow(2.0, (double)config.directory_index_bits);
    config.page_table_entry_count = (uint32_t)pow(2.0, (double)config.page_table_index_bits);
    return config;
}

bool memory_config_is_valid(uint32_t page_size, uint32_t physical_memory_size) {
    if (!is_power_of_two(page_size)) {
        return false;
    }
    if (page_size < SIM_MIN_PAGE_SIZE || page_size > SIM_MAX_PAGE_SIZE) {
        return false;
    }
    if (physical_memory_size < SIM_MIN_PHYSICAL_MEMORY_SIZE ||
        physical_memory_size > SIM_MAX_PHYSICAL_MEMORY_SIZE) {
        return false;
    }
    if (physical_memory_size % page_size != 0u) {
        return false;
    }
    return true;
}

uint32_t memory_config_compute_offset_bits(uint32_t page_size) {
    uint32_t bits = 0u;
    while (page_size > 1u) {
        page_size >>= 1u;
        bits++;
    }
    return bits;
}

uint32_t memory_config_compute_directory_index_bits(uint32_t vpn_bits) {
    return (vpn_bits + 1u) / 2u;
}

