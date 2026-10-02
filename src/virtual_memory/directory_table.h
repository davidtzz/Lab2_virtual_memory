#ifndef DIRECTORY_TABLE_H
#define DIRECTORY_TABLE_H
#include "sim_virtual.h"
#include <stdint.h>

DirectoryTable directory_table_create(uint32_t entry_count);
void directory_table_destroy(DirectoryTable *directory);
PageTable *directory_table_get_or_create_page_table(DirectoryTable *directory, uint32_t directory_index, uint32_t page_table_entry_count);
PageTable *directory_table_get_page_table(const DirectoryTable *directory, uint32_t directory_index);

#endif