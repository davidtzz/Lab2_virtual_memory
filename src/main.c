#include <stdio.h>
#include <stdlib.h>
#include "cli.h"
#include "commands.h"
#include "config.h"
#include "vmm.h"

int main(int argc, char **argv)
{
    cli_options_t options;
    int args_result = cli_parse_args(argc, argv, &options);
    if (args_result != 0)
        return args_result > 0 ? 0 : 1;

    vm_config_t cfg;
    if (config_init(&cfg, options.page_size, options.phys_bytes,
                    options.verbose) != 0)
        return 1;

    FILE *in = stdin;
    if (options.path) {
        in = fopen(options.path, "r");
        if (!in) {
            perror(options.path);
            return 1;
        }
    }

    vmm_t *vm = vmm_create(&cfg);
    if (!vm) {
        fprintf(stderr, "Error: no se pudo crear el simulador (sin memoria).\n");
        if (in != stdin) fclose(in);
        return 1;
    }

    if (options.verbose)
        vmm_print_address_layout(&cfg, stdout);

    unsigned long errors = commands_run(vm, in, options.quiet);

    if (options.verbose)
        vmm_dump_pagetable(vm, stdout);
    vmm_print_stats(vm, stdout);
    if (errors > 0)
        fprintf(stderr, "\nSe encontraron %lu operaciones con error.\n", errors);

    vmm_destroy(vm);
    if (in != stdin)
        fclose(in);
    return 0;
}
