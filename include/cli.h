#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include "config.h"

typedef struct {
    uint32_t phys_bytes;
    uint32_t page_size;
    int verbose;
    int quiet;
    const char *path;
} cli_options_t;

/* Devuelve 0 si continúa, 1 si se solicitó ayuda y -1 si hubo un error. */
int cli_parse_args(int argc, char **argv, cli_options_t *options);

#endif