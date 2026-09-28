#include <stdio.h>
#include <string.h>
#include "cli.h"
#include "parse.h"

static void usage(const char *prog)
{
    fprintf(stderr,
        "Uso: %s [opciones] [archivo]\n"
        "  -m <KB>       memoria física en KB, mínimo 256 (por defecto: 256)\n"
        "  -s <bytes>    tamaño de página, potencia de 2 (por defecto: 4096)\n"
        "  -v            modo verboso: muestra cada fallo y reemplazo\n"
        "  -q            silencioso: no imprime cada operación\n"
        "  -h            esta ayuda\n"
        "Si no se indica archivo se lee de la entrada estándar.\n", prog);
}

int cli_parse_args(int argc, char **argv, cli_options_t *options)
{
    uint64_t number;
    options->phys_bytes = DEFAULT_PHYS_BYTES;
    options->page_size = DEFAULT_PAGE_SIZE;
    options->verbose = 0;
    options->quiet = 0;
    options->path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &number) != 0 || number == 0 ||
                number > 4194303u) {
                fprintf(stderr, "Valor inválido para -m\n");
                return -1;
            }
            options->phys_bytes = (uint32_t)(number * 1024u);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &number) != 0 || number > UINT32_MAX) {
                fprintf(stderr, "Valor inválido para -s\n");
                return -1;
            }
            options->page_size = (uint32_t)number;
        } else if (strcmp(argv[i], "-v") == 0) {
            options->verbose = 1;
        } else if (strcmp(argv[i], "-q") == 0) {
            options->quiet = 1;
        } else if (strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 1;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "Opción desconocida: %s\n", argv[i]);
            usage(argv[0]);
            return -1;
        } else {
            options->path = argv[i];
        }
    }
    return 0;
}