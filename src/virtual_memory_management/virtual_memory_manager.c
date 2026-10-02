#include "sim_virtual.h"
#include <stdio.h>
#include <stdlib.h>

static uint64_t round_up_div(uint64_t value, uint64_t divisor) {
    if (divisor == 0u) {
        return 0u;
    }
    return (value + divisor - 1u) / divisor;
}

static PageTableEntry *memory_manager_get_page_entry(MemoryManager *manager, const VirtualAddress *va) {
    if (manager == NULL || va == NULL) {
        return NULL;
    }
    PageTable *table = directory_table_get_page_table(&manager->directory, va->directory_index);
    if (table == NULL) {
        return NULL;
    }
    return page_table_get_entry(table, va->page_table_index);
}

static uint32_t memory_manager_choose_victim_frame(MemoryManager *manager, const VirtualAddress *va) {
    (void)va;
    if (manager == NULL) {
        return 0u;
    }
    uint32_t victim = UINT32_MAX;
    uint64_t oldest = UINT64_MAX;
    for (uint32_t frame = 0; frame < manager->frame_table.frame_count; ++frame) {
        const FrameInfo *info = frame_table_get_frame_info(&manager->frame_table, frame);
        if (info == NULL || !info->is_allocated) {
            continue;
        }
        for (uint32_t i = 0; i < SIM_TLB_SIZE; ++i) {
            if (manager->tlb.entries[i].valid && manager->tlb.entries[i].vpn == info->vpn) {
                if (manager->tlb.entries[i].last_access < oldest) {
                    oldest = manager->tlb.entries[i].last_access;
                    victim = frame;
                }
            }
        }
        if (victim == UINT32_MAX) {
            victim = frame;
        }
    }
    if (victim == UINT32_MAX) {
        fprintf(stderr, "No se pudo elegir victima de reemplazo\n");
        exit(1);
    }
    return victim;
}

static void memory_manager_handle_page_fault(MemoryManager *manager, const VirtualAddress *va) {
    PageTableEntry *entry = memory_manager_get_page_entry(manager, va);
    if (entry == NULL || !entry->allocated_bit) {
        fprintf(stderr, "Segmentation fault en direccion virtual %u\n", va->raw);
        exit(1);
    }

    uint32_t frame = 0u;
    bool replaced = false;
    if (frame_table_has_free_frame(&manager->frame_table)) {
        frame = frame_table_allocate_free_frame(&manager->frame_table, va->vpn, va->directory_index, va->page_table_index);
    } else {
        frame = memory_manager_choose_victim_frame(manager, va);
        const FrameInfo *victim_info = frame_table_get_frame_info(&manager->frame_table, frame);
        if (victim_info != NULL && victim_info->is_allocated) {
            PageTable *victim_table = directory_table_get_page_table(&manager->directory, victim_info->directory_index);
            if (victim_table != NULL) {
                PageTableEntry *victim_entry = page_table_get_entry(victim_table, victim_info->page_table_index);
                if (victim_entry != NULL) {
                    page_table_entry_invalidate(victim_entry);
                    tlb_invalidate_vpn(&manager->tlb, victim_info->vpn);
                    replaced = true;
                }
            }
        }
        frame_table_free_frame(&manager->frame_table, frame);
        frame = frame_table_allocate_free_frame(&manager->frame_table, va->vpn, va->directory_index, va->page_table_index);
    }

    const uint32_t physical_base = frame * manager->config.page_size;
    physical_memory_clear_range(&manager->physical_memory, physical_base, manager->config.page_size);
    if (replaced) {
        stats_record_replacement(&manager->stats);
    }
    tlb_insert(&manager->tlb, va->vpn, frame, manager->clock);
    if (entry != NULL) {
        page_table_entry_load(entry, frame);
    }
}

MemoryManager memory_manager_create(const MemoryConfig *config, SimPolicy policy) {
    MemoryManager manager;
    manager.config = *config;
    manager.directory = directory_table_create(config->directory_entry_count);
    manager.physical_memory = (PhysicalMemory){0};
    physical_memory_init(&manager.physical_memory, config->physical_memory_size);
    frame_table_init(&manager.frame_table, config->frame_count);
    tlb_init(&manager.tlb);
    manager.next_virtual_address = 0u;
    stats_reset(&manager.stats);
    manager.clock = 0u;
    manager.policy = policy;
    return manager;
}

void memory_manager_destroy(MemoryManager *manager) {
    if (manager == NULL) {
        return;
    }
    directory_table_destroy(&manager->directory);
    physical_memory_destroy(&manager->physical_memory);
    frame_table_destroy(&manager->frame_table);
}

uint32_t memory_manager_allocate(MemoryManager *manager, uint32_t bytes) {
    if (manager == NULL) {
        return 0u;
    }
    if (bytes == 0u) {
        fprintf(stderr, "La cantidad de bytes a reservar debe ser mayor a 0\n");
        exit(1);
    }

    const uint64_t page_size = manager->config.page_size;
    const uint64_t pages = round_up_div(bytes, page_size);
    const uint64_t reserved_bytes = pages * page_size;
    const uint64_t address_limit = (uint64_t)1 << SIM_VIRTUAL_ADDRESS_BITS;
    if (manager->next_virtual_address + reserved_bytes > address_limit) {
        fprintf(stderr, "No hay suficiente espacio virtual\n");
        exit(1);
    }

    const uint32_t start_address = (uint32_t)manager->next_virtual_address;
    for (uint64_t page_index = 0; page_index < pages; ++page_index) {
        const uint32_t page_address = (uint32_t)(start_address + page_index * page_size);
        const VirtualAddress va = virtual_address_create(page_address, &manager->config);
        PageTable *table = directory_table_get_or_create_page_table(&manager->directory, va.directory_index, manager->config.page_table_entry_count);
        PageTableEntry *entry = page_table_get_entry(table, va.page_table_index);
        if (entry == NULL) {
            fprintf(stderr, "No se pudo obtener la entrada de pagina\n");
            exit(1);
        }
        if (entry->allocated_bit) {
            fprintf(stderr, "La pagina ya esta reservada\n");
            exit(1);
        }
        page_table_entry_allocate(entry);
    }

    manager->next_virtual_address += (uint32_t)reserved_bytes;
    return start_address;
}

void memory_manager_write(MemoryManager *manager, uint32_t virtual_address, uint8_t value) {
    if (manager == NULL) {
        return;
    }
    const VirtualAddress va = virtual_address_create(virtual_address, &manager->config);
    uint32_t frame = 0u;
    bool tlb_hit = tlb_lookup(&manager->tlb, va.vpn, &frame);
    stats_record_access(&manager->stats);
    manager->clock++;
    if (tlb_hit) {
        stats_record_tlb_hit(&manager->stats);
    }

    PageTableEntry *entry = memory_manager_get_page_entry(manager, &va);
    if (entry == NULL || !entry->allocated_bit) {
        fprintf(stderr, "Segmentation fault en direccion virtual %u\n", virtual_address);
        exit(1);
    }
    if (!entry->valid_bit) {
        stats_record_page_fault(&manager->stats);
        memory_manager_handle_page_fault(manager, &va);
        frame = entry->pfn;
    }
    if (tlb_hit) {
        // El frame ya es valido y la TLB ya lo tiene
    } else {
        tlb_insert(&manager->tlb, va.vpn, entry->pfn, manager->clock);
    }
    page_table_entry_record_access(entry, true);
    const uint32_t physical_address = frame * manager->config.page_size + va.offset;
    physical_memory_write_byte(&manager->physical_memory, physical_address, value);
}

uint8_t memory_manager_read(MemoryManager *manager, uint32_t virtual_address) {
    if (manager == NULL) {
        return 0u;
    }
    const VirtualAddress va = virtual_address_create(virtual_address, &manager->config);
    uint32_t frame = 0u;
    bool tlb_hit = tlb_lookup(&manager->tlb, va.vpn, &frame);
    stats_record_access(&manager->stats);
    manager->clock++;
    if (tlb_hit) {
        stats_record_tlb_hit(&manager->stats);
    }

    PageTableEntry *entry = memory_manager_get_page_entry(manager, &va);
    if (entry == NULL || !entry->allocated_bit) {
        fprintf(stderr, "Segmentation fault en direccion virtual %u\n", virtual_address);
        exit(1);
    }
    if (!entry->valid_bit) {
        stats_record_page_fault(&manager->stats);
        memory_manager_handle_page_fault(manager, &va);
        frame = entry->pfn;
    }
    if (!tlb_hit) {
        tlb_insert(&manager->tlb, va.vpn, entry->pfn, manager->clock);
    }
    page_table_entry_record_access(entry, false);
    const uint32_t physical_address = frame * manager->config.page_size + va.offset;
    return physical_memory_read_byte(&manager->physical_memory, physical_address);
}

void memory_manager_free(MemoryManager *manager, uint32_t virtual_address) {
    if (manager == NULL) {
        return;
    }
    const VirtualAddress va = virtual_address_create(virtual_address, &manager->config);
    PageTable *table = directory_table_get_page_table(&manager->directory, va.directory_index);
    if (table == NULL) {
        fprintf(stderr, "La direccion virtual no esta asignada\n");
        exit(1);
    }
    PageTableEntry *entry = page_table_get_entry(table, va.page_table_index);
    if (entry == NULL || !entry->valid_bit) {
        fprintf(stderr, "La pagina virtual no esta cargada en memoria\n");
        exit(1);
    }

    const uint32_t frame = entry->pfn;
    page_table_entry_invalidate(entry);
    tlb_invalidate_vpn(&manager->tlb, va.vpn);
    frame_table_free_frame(&manager->frame_table, frame);
}
