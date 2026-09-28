/*
 * config.h - Constantes y configuración global del simulador.
 *
 * La configuración (tamaño de página, memoria física, política) se fija una
 * sola vez al inicio con config_init() y de ahí en adelante es de solo lectura.
 * A partir del tamaño de página se derivan los bits de la dirección virtual:
 *
 *   4 KB (por defecto):  | PT1 (10b) | PT2 (10b) | Offset (12b) |
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define VA_BITS            32u              /* Espacio virtual: 32 bits        */
#define MIN_PAGE_SIZE      256u
#define MAX_PAGE_SIZE      65536u
#define DEFAULT_PAGE_SIZE  4096u            /* 4 KB                            */
#define MIN_PHYS_BYTES     (256u * 1024u)   /* Mínimo exigido: 256 KB          */
#define DEFAULT_PHYS_BYTES (256u * 1024u)

/* Modelo de tiempo simulado (nanosegundos). Solo sirve para comparar políticas. */
#define T_MEM_NS    100ULL        /* un acceso a RAM                           */
#define T_FAULT_NS  2000ULL       /* costo fijo de atender un fallo (trap)     */
#define T_DISK_NS   5000000ULL    /* una lectura o escritura a disco (5 ms)    */

typedef struct {
    uint32_t page_size;     /* bytes por página (potencia de 2)                */
    uint32_t offset_bits;   /* log2(page_size)                                 */
    uint32_t pt1_bits;      /* bits del índice de nivel 1                      */
    uint32_t pt2_bits;      /* bits del índice de nivel 2                      */
    uint32_t phys_bytes;    /* tamaño de la memoria física                     */
    uint32_t num_frames;    /* phys_bytes / page_size                          */
    int      verbose;       /* 1 = imprimir cada fallo y reemplazo             */
} vm_config_t;

/* Valida los parámetros y calcula los campos derivados. 0 = ok, -1 = error. */
int config_init(vm_config_t *cfg, uint32_t page_size, uint32_t phys_bytes,
                int verbose);

#endif /* CONFIG_H */
