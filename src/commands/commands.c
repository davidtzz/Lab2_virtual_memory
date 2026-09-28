#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "commands.h"
#include "parse.h"

#define TOK_LEN 64

typedef int (*command_handler_t)(vmm_t *, FILE *, const char *,
                                 unsigned long, int);

typedef struct {
    const char *name;
    command_handler_t handler;
} command_entry_t;

static int read_token(FILE *input, char token[TOK_LEN])
{
    return fscanf(input, "%63s", token) == 1 ? 0 : -1;
}

static int read_number(FILE *input, uint64_t *out, const char *command,
                       unsigned long op_no)
{
    char token[TOK_LEN];
    if (read_token(input, token) != 0) {
        fprintf(stderr, "Error (op #%lu): falta un argumento para '%s'.\n",
                op_no, command);
        return -1;
    }
    if (parse_u64(token, out) != 0) {
        fprintf(stderr, "Error (op #%lu): número inválido '%s' en '%s'.\n",
                op_no, token, command);
        return -1;
    }
    return 0;
}

static int read_address(FILE *input, uint64_t *address, const char *command,
                        unsigned long op_no)
{
    if (read_number(input, address, command, op_no) != 0)
        return -1;
    if (*address > UINT32_MAX) {
        fprintf(stderr, "Error (op #%lu): dirección fuera de 32 bits.\n", op_no);
        return -1;
    }
    return 0;
}

static int command_alloc(vmm_t *vm, FILE *input, const char *command,
                         unsigned long op_no, int quiet)
{
    uint64_t bytes;
    uint32_t va;
    if (read_number(input, &bytes, command, op_no) != 0 ||
        vmm_alloc(vm, bytes, &va) != 0)
        return 1;
    if (!quiet) {
        if (vm->cfg.verbose)
            printf("[alloc] reservados %" PRIu64 " bytes; base VA %" PRIu32
                   ", rango VA [ %" PRIu32 ", %" PRIu64 " ]\n",
                   bytes, va, va, (uint64_t)va + bytes - 1u);
        else
            printf("[alloc] %" PRIu64 " bytes -> VA 0x%08" PRIx32 "\n", bytes, va);
    }
    return 0;
}

static int command_write(vmm_t *vm, FILE *input, const char *command,
                         unsigned long op_no, int quiet)
{
    uint64_t address, value;
    if (read_number(input, &address, command, op_no) != 0 ||
        read_number(input, &value, command, op_no) != 0)
        return 1;
    if (address > UINT32_MAX) {
        fprintf(stderr, "Error (op #%lu): dirección fuera de 32 bits.\n", op_no);
        return 1;
    }
    if (value > 255)
        fprintf(stderr, "Aviso (op #%lu): valor %" PRIu64 " truncado a 8 bits "
                "(cada dirección guarda 1 byte).\n", op_no, value);
    if (!quiet && vm->cfg.verbose)
        printf("[operación] write solicita VA %" PRIu32 " con valor %u\n",
               (uint32_t)address, (unsigned)(value & 0xFFu));
    if (vmm_write(vm, (uint32_t)address, (uint8_t)(value & 0xFFu)) != 0)
        return 1;
    if (!quiet) {
        if (vm->cfg.verbose)
            printf("[write] VA %" PRIu32 " = %u\n", (uint32_t)address,
                   (unsigned)(value & 0xFFu));
        else
            printf("[write] VA 0x%08" PRIx32 " = %u\n", (uint32_t)address,
                   (unsigned)(value & 0xFFu));
    }
    return 0;
}

static int command_read(vmm_t *vm, FILE *input, const char *command,
                        unsigned long op_no, int quiet)
{
    uint64_t address;
    uint8_t value;
    if (read_address(input, &address, command, op_no) != 0)
        return 1;
    if (!quiet && vm->cfg.verbose)
        printf("[operación] read solicita VA %" PRIu32 "\n", (uint32_t)address);
    if (vmm_read(vm, (uint32_t)address, &value) != 0)
        return 1;
    if (!quiet) {
        if (vm->cfg.verbose)
            printf("[read ] VA %" PRIu32 " -> %u\n", (uint32_t)address,
                   (unsigned)value);
        else
            printf("[read ] VA 0x%08" PRIx32 " -> %u\n", (uint32_t)address,
                   (unsigned)value);
    }
    return 0;
}

static int command_free(vmm_t *vm, FILE *input, const char *command,
                        unsigned long op_no, int quiet)
{
    uint64_t address;
    if (read_address(input, &address, command, op_no) != 0 ||
        vmm_free(vm, (uint32_t)address) != 0)
        return 1;
    if (!quiet) {
        if (vm->cfg.verbose)
            printf("[free ] VA %" PRIu32 "\n", (uint32_t)address);
        else
            printf("[free ] VA 0x%08" PRIx32 "\n", (uint32_t)address);
    }
    return 0;
}

static int command_dump(vmm_t *vm, FILE *input, const char *command,
                        unsigned long op_no, int quiet)
{
    (void)input;
    (void)command;
    (void)op_no;
    (void)quiet;
    vmm_dump_pagetable(vm, stdout);
    return 0;
}

static const command_entry_t command_table[] = {
    { "alloc", command_alloc },
    { "write", command_write },
    { "read", command_read },
    { "free", command_free },
    { "dump", command_dump }
};

static int execute_command(vmm_t *vm, FILE *input, const char *command,
                           unsigned long op_no, int quiet)
{
    for (size_t i = 0; i < sizeof command_table / sizeof command_table[0]; i++) {
        if (strcmp(command, command_table[i].name) == 0)
            return command_table[i].handler(vm, input, command, op_no, quiet);
    }
    fprintf(stderr, "Error (op #%lu): comando desconocido '%s'.\n", op_no, command);
    return 1;
}

unsigned long commands_run(vmm_t *vm, FILE *input, int quiet)
{
    char token[TOK_LEN];
    unsigned long op_no = 0;
    unsigned long errors = 0;
    while (read_token(input, token) == 0) {
        if (token[0] == '#') {
            int character;
            while ((character = fgetc(input)) != EOF && character != '\n')
                ;
            continue;
        }
        op_no++;
        errors += (unsigned long)execute_command(vm, input, token, op_no, quiet);
    }
    return errors;
}