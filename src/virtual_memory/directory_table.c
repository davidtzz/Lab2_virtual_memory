#include "sim_virtual.h"
#include <stdio.h>

DirectoryTable directory_table_create(uint32_t entry_count) {
    DirectoryTable directory;
    directory.count = entry_count;
    directory.entries = (DirectoryEntry *)calloc(entry_count, sizeof(DirectoryEntry));
    if (directory.entries == NULL) {
        directory.count = 0u;
        return directory;
    }
    return directory;
}

void directory_table_destroy(DirectoryTable *directory) {
    if (directory == NULL) {
        return;
    }
    for (uint32_t i = 0; i < directory->count; ++i) {
        if (directory->entries[i].has_table && directory->entries[i].table != NULL) {
            page_table_destroy(directory->entries[i].table);
        }
    }
    free(directory->entries);
    directory->entries = NULL;
    directory->count = 0u;
}

PageTable *directory_table_get_or_create_page_table(DirectoryTable *directory, uint32_t directory_index, uint32_t page_table_entry_count) {
    if (directory == NULL) {
        return NULL;
    }
    if (directory_index >= directory->count) {
        fprintf(stderr, "Indice de directorio fuera de rango: %u\n", directory_index);
        exit(1);
    }
    if (!directory->entries[directory_index].has_table) {
        directory->entries[directory_index].table = page_table_create(page_table_entry_count);
        directory->entries[directory_index].has_table = true;
    }
    return directory->entries[directory_index].table;
}

PageTable *directory_table_get_page_table(const DirectoryTable *directory, uint32_t directory_index) {
    if (directory == NULL || directory_index >= directory->count) {
        return NULL;
    }
    return directory->entries[directory_index].table;
}
