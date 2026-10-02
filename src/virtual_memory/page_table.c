#include "sim_virtual.h"
#include <stdio.h>
#include <stdlib.h>

void page_table_entry_init(PageTableEntry *entry) {
    if (entry == NULL) {
        return;
    }
    entry->pfn = 0u;
    entry->valid_bit = false;
    entry->accessed_bit = false;
    entry->dirty_bit = false;
    entry->allocated_bit = false;
}

void page_table_entry_allocate(PageTableEntry *entry) {
    if (entry == NULL) {
        return;
    }
    if (entry->allocated_bit) {
        fprintf(stderr, "La pagina ya esta reservada\n");
        exit(1);
    }
    entry->allocated_bit = true;
}

void page_table_entry_load(PageTableEntry *entry, uint32_t pfn) {
    if (entry == NULL) {
        return;
    }
    if (entry->valid_bit) {
        fprintf(stderr, "La pagina ya esta cargada en el marco %u\n", entry->pfn);
        exit(1);
    }
    entry->pfn = pfn;
    entry->valid_bit = true;
    entry->accessed_bit = false;
    entry->dirty_bit = false;
}

void page_table_entry_record_access(PageTableEntry *entry, bool is_write) {
    if (entry == NULL) {
        return;
    }
    if (!entry->valid_bit) {
        fprintf(stderr, "No se puede registrar un acceso a una pagina no cargada\n");
        exit(1);
    }
    entry->accessed_bit = true;
    if (is_write) {
        entry->dirty_bit = true;
    }
}

void page_table_entry_invalidate(PageTableEntry *entry) {
    if (entry == NULL) {
        return;
    }
    entry->pfn = 0u;
    entry->valid_bit = false;
    entry->accessed_bit = false;
    entry->dirty_bit = false;
}

PageTable *page_table_create(uint32_t entry_count) {
    PageTable *table = (PageTable *)calloc(1u, sizeof(PageTable));
    if (table == NULL) {
        return NULL;
    }
    table->count = entry_count;
    table->entries = (PageTableEntry *)calloc(entry_count, sizeof(PageTableEntry));
    if (table->entries == NULL) {
        free(table);
        return NULL;
    }
    for (uint32_t i = 0; i < entry_count; ++i) {
        page_table_entry_init(&table->entries[i]);
    }
    return table;
}

void page_table_destroy(PageTable *table) {
    if (table == NULL) {
        return;
    }
    free(table->entries);
    free(table);
}

PageTableEntry *page_table_get_entry(PageTable *table, uint32_t index) {
    if (table == NULL) {
        return NULL;
    }
    if (index >= table->count) {
        return NULL;
    }
    return &table->entries[index];
}


