/*
 * config.c - Validación de parámetros y cálculo de los campos derivados.
 */
#include <stdio.h>
#include "config.h"

static int is_pow2(uint32_t x)
{
    return x != 0 && (x & (x - 1u)) == 0;
}

int config_init(vm_config_t *cfg, uint32_t page_size, uint32_t phys_bytes,
                int verbose)
{
    if (!is_pow2(page_size) || page_size < MIN_PAGE_SIZE ||
        page_size > MAX_PAGE_SIZE) {
        fprintf(stderr,
                "Error: tamaño de página inválido (%u). Debe ser potencia de 2 "
                "entre %u y %u.\n",
                (unsigned)page_size, MIN_PAGE_SIZE, MAX_PAGE_SIZE);
        return -1;
    }
    if (phys_bytes < MIN_PHYS_BYTES) {
        fprintf(stderr, "Error: la memoria física mínima es %u KB.\n",
                MIN_PHYS_BYTES / 1024u);
        return -1;
    }
    if (phys_bytes % page_size != 0) {
        fprintf(stderr,
                "Error: la memoria física debe ser múltiplo del tamaño de "
                "página.\n");
        return -1;
    }

    cfg->page_size = page_size;

    /* offset_bits = log2(page_size) */
    uint32_t bits = 0;
    while ((1u << bits) < page_size)
        bits++;
    cfg->offset_bits = bits;

    /* Los bits restantes de la VA se reparten entre los dos niveles.
     * Con 4 KB: 32 - 12 = 20 bits -> 10 (PT1) + 10 (PT2). */
    uint32_t rest = VA_BITS - bits;
    cfg->pt2_bits = rest / 2u;
    cfg->pt1_bits = rest - cfg->pt2_bits;

    cfg->phys_bytes = phys_bytes;
    cfg->num_frames = phys_bytes / page_size;
    cfg->verbose    = verbose;
    return 0;
}
