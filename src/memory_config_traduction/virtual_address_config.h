#ifndef VIRTUAL_ADDRESS_CONFIG_H
#define VIRTUAL_ADDRESS_CONFIG_H

#include <stdint.h>

VirtualAddress virtual_address_create(uint32_t raw, const MemoryConfig *config);

uint32_t virtual_address_get_directory_index(const VirtualAddress *va, const MemoryConfig *config);

uint32_t virtual_address_get_page_table_index(const VirtualAddress *va, const MemoryConfig *config);

uint32_t virtual_address_get_offset(const VirtualAddress *va, const MemoryConfig *config);

uint32_t virtual_address_get_vpn(const VirtualAddress *va, const MemoryConfig *config);
#endif // VIRTUAL_ADDRESS_CONFIG_H