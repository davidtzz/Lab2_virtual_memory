#include "sim_virtual.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_summary(const MemoryManager *manager, size_t instruction_count) {
    printf("instrucciones=%zu\n", instruction_count);
    printf("accesos=%llu\n", (unsigned long long)manager->stats.total_accesses);
    printf("fallos_pagina=%llu\n", (unsigned long long)manager->stats.page_faults);
    printf("reemplazos=%llu\n", (unsigned long long)manager->stats.replacements);
    printf("hits_tlb=%llu\n", (unsigned long long)manager->stats.tlb_hits);
    printf("ticks=%llu\n", (unsigned long long)manager->clock);
    printf("policy=%s\n", manager->policy == SIM_POLICY_FIFO ? "FIFO" : "LRU");
}

int main(int argc, char **argv) {
    SimulationConfig config;
    simulation_config_default(&config);

    const char *file_name = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--page-size") == 0 && i + 1 < argc) {
            config.page_size = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--memory") == 0 && i + 1 < argc) {
            config.physical_memory_size = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (file_name == NULL) {
            file_name = argv[i];
        } else {
            simulation_config_print_usage(argv[0]);
            return 1;
        }
    }

    if (file_name == NULL) {
        simulation_config_print_usage(argv[0]);
        return 1;
    }

    config.file_name = (char *)file_name;
    MemoryConfig memory_config = memory_config_create(config.page_size, config.physical_memory_size);
    MemoryManager manager = memory_manager_create(&memory_config, config.policy);

    Instruction *instructions = NULL;
    size_t instruction_count = 0;
    if (!sim_load_program(file_name, &instructions, &instruction_count)) {
        memory_manager_destroy(&manager);
        return 1;
    }

    for (size_t i = 0; i < instruction_count; ++i) {
        Instruction *current = &instructions[i];
        switch (current->type) {
            case SIM_INSTR_ALLOC:
                memory_manager_allocate(&manager, current->operand);
                break;
            case SIM_INSTR_WRITE:
                memory_manager_write(&manager, current->operand, current->value);
                break;
            case SIM_INSTR_READ:
                memory_manager_read(&manager, current->operand);
                break;
            case SIM_INSTR_FREE:
                memory_manager_free(&manager, current->operand);
                break;
            default:
                fprintf(stderr, "Tipo de instruccion desconocido\n");
                break;
        }
    }

    print_summary(&manager, instruction_count);

    sim_free_program(instructions);
    memory_manager_destroy(&manager);
    return 0;
}
