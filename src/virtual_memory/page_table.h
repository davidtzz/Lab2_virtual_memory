#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H
#include "sim_virtual.h"

void page_table_entry_init(PageTableEntry *entry);
void page_table_entry_allocate(PageTableEntry *entry);
void page_table_entry_load(PageTableEntry *entry, uint32_t pfn);
void page_table_entry_record_access(PageTableEntry *entry, bool is_write);
void page_table_entry_invalidate(PageTableEntry *entry);

#endif