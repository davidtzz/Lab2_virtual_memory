#include "sim_virtual.h"
#include <stdio.h>
#include <stdlib.h>

VirtualAddress virtual_address_create(uint32_t raw, const MemoryConfig *config) {
    VirtualAddress va;
    va.raw = raw;
    va.offset = raw % config->page_size;
    va.vpn = raw >> config->offset_bits;
    va.directory_index = va.vpn >> config->page_table_index_bits;
    va.page_table_index = va.vpn & (config->page_table_entry_count - 1u);
    return va;
}

uint32_t virtual_address_get_directory_index(const VirtualAddress *va, const MemoryConfig *config) {
    (void)config;
    return va->directory_index;
}

uint32_t virtual_address_get_page_table_index(const VirtualAddress *va, const MemoryConfig *config) {
    (void)config;
    return va->page_table_index;
}

uint32_t virtual_address_get_offset(const VirtualAddress *va, const MemoryConfig *config) {
    (void)config;
    return va->offset;
}

uint32_t virtual_address_get_vpn(const VirtualAddress *va, const MemoryConfig *config) {
    (void)config;
    return va->vpn;
}

