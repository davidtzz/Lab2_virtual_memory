#include "sim_virtual.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//modules
#include "memory_config_traduction/memory_config.h"
#include "memory_config_traduction/virtual_address_config.h"
#include "virtual_memory/directory_table.h"
#include "virtual_memory/page_table.h"
#include "physical_memory/frame_table.h"
#include "physical_memory/physical_memory.h"
#include "physical_memory/tlb.h"
#include "statistics/statistics.h"
#include "virtual_memory_management/virtual_memory_manager.h"


static bool is_power_of_two(uint32_t value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

static uint64_t round_up_div(uint64_t value, uint64_t divisor) {
    if (divisor == 0u) {
        return 0u;
    }
    return (value + divisor - 1u) / divisor;
}

bool sim_parse_instruction_line(const char *line, Instruction *out_instruction) {
    if (line == NULL || out_instruction == NULL) {
        return false;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", line);
    char *token = strtok(buffer, " \t\r\n");
    if (token == NULL) {
        return false;
    }

    if (strcmp(token, "alloc") == 0) {
        char *value = strtok(NULL, " \t\r\n");
        if (value == NULL) {
            return false;
        }
        out_instruction->type = SIM_INSTR_ALLOC;
        out_instruction->operand = (uint32_t)strtoul(value, NULL, 10);
        out_instruction->value = 0u;
        return true;
    }
    if (strcmp(token, "write") == 0) {
        char *addr = strtok(NULL, " \t\r\n");
        char *value = strtok(NULL, " \t\r\n");
        if (addr == NULL || value == NULL) {
            return false;
        }
        out_instruction->type = SIM_INSTR_WRITE;
        out_instruction->operand = (uint32_t)strtoul(addr, NULL, 10);
        out_instruction->value = (uint8_t)strtoul(value, NULL, 10);
        return true;
    }
    if (strcmp(token, "read") == 0) {
        char *addr = strtok(NULL, " \t\r\n");
        if (addr == NULL) {
            return false;
        }
        out_instruction->type = SIM_INSTR_READ;
        out_instruction->operand = (uint32_t)strtoul(addr, NULL, 10);
        out_instruction->value = 0u;
        return true;
    }
    if (strcmp(token, "free") == 0) {
        char *addr = strtok(NULL, " \t\r\n");
        if (addr == NULL) {
            return false;
        }
        out_instruction->type = SIM_INSTR_FREE;
        out_instruction->operand = (uint32_t)strtoul(addr, NULL, 10);
        out_instruction->value = 0u;
        return true;
    }
    return false;
}

bool sim_load_program(const char *file_name, Instruction **instructions_out, size_t *count_out) {
    if (file_name == NULL || instructions_out == NULL || count_out == NULL) {
        return false;
    }

    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        fprintf(stderr, "No se pudo abrir %s: %s\n", file_name, strerror(errno));
        return false;
    }

    Instruction *instructions = NULL;
    size_t count = 0u;
    size_t capacity = 16u;
    instructions = (Instruction *)calloc(capacity, sizeof(Instruction));
    if (instructions == NULL) {
        fclose(file);
        return false;
    }

    char line[512];
    while (fgets(line, sizeof(line), file) != NULL) {
        char trimmed[512];
        size_t i = 0u;
        while (line[i] != '\0' && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')) {
            i++;
        }
        if (line[i] == '#' || line[i] == '\0') {
            continue;
        }

        size_t j = 0u;
        while (line[i] != '\0' && j < sizeof(trimmed) - 1u) {
            trimmed[j++] = line[i++];
        }
        trimmed[j] = '\0';

        char *working = (char *)malloc(strlen(trimmed) + 1u);
        if (working == NULL) {
            free(instructions);
            fclose(file);
            return false;
        }
        strcpy(working, trimmed);

        char *token = strtok(working, " \t\r\n");
        while (token != NULL) {
            if (count == capacity) {
                capacity *= 2u;
                Instruction *tmp = (Instruction *)realloc(instructions, capacity * sizeof(Instruction));
                if (tmp == NULL) {
                    free(working);
                    free(instructions);
                    fclose(file);
                    return false;
                }
                instructions = tmp;
            }

            if (strcmp(token, "alloc") == 0) {
                char *value = strtok(NULL, " \t\r\n");
                if (value == NULL) {
                    free(working);
                    free(instructions);
                    fclose(file);
                    return false;
                }
                instructions[count].type = SIM_INSTR_ALLOC;
                instructions[count].operand = (uint32_t)strtoul(value, NULL, 10);
                instructions[count].value = 0u;
                count++;
            } else if (strcmp(token, "write") == 0) {
                char *address = strtok(NULL, " \t\r\n");
                char *value = strtok(NULL, " \t\r\n");
                if (address == NULL || value == NULL) {
                    free(working);
                    free(instructions);
                    fclose(file);
                    return false;
                }
                instructions[count].type = SIM_INSTR_WRITE;
                instructions[count].operand = (uint32_t)strtoul(address, NULL, 10);
                instructions[count].value = (uint8_t)strtoul(value, NULL, 10);
                count++;
            } else if (strcmp(token, "read") == 0) {
                char *address = strtok(NULL, " \t\r\n");
                if (address == NULL) {
                    free(working);
                    free(instructions);
                    fclose(file);
                    return false;
                }
                instructions[count].type = SIM_INSTR_READ;
                instructions[count].operand = (uint32_t)strtoul(address, NULL, 10);
                instructions[count].value = 0u;
                count++;
            } else if (strcmp(token, "free") == 0) {
                char *address = strtok(NULL, " \t\r\n");
                if (address == NULL) {
                    free(working);
                    free(instructions);
                    fclose(file);
                    return false;
                }
                instructions[count].type = SIM_INSTR_FREE;
                instructions[count].operand = (uint32_t)strtoul(address, NULL, 10);
                instructions[count].value = 0u;
                count++;
            } else {
                free(working);
                free(instructions);
                fclose(file);
                return false;
            }
            token = strtok(NULL, " \t\r\n");
        }

        free(working);
    }

    fclose(file);
    *instructions_out = instructions;
    *count_out = count;
    return true;
}

void sim_free_program(Instruction *instructions) {
    free(instructions);
}

void simulation_config_default(SimulationConfig *config) {
    if (config == NULL) {
        return;
    }
    config->file_name = NULL;
    config->page_size = 4096u;
    config->physical_memory_size = 262144u;
    config->policy = SIM_POLICY_FIFO;
}

void simulation_config_print_usage(const char *program_name) {
    fprintf(stderr, "Uso: %s <archivo> [--page-size N] [--memory N]\n", program_name);
}

