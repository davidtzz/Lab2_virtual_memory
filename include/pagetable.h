/*
 * pagetable.h - Tabla de páginas de dos niveles.
 *
 *   Nivel 1 (l1): arreglo de 2^pt1_bits punteros a tablas de nivel 2.
 *                 Un puntero NULL significa "esta tabla L2 no existe todavía".
 *   Nivel 2     : arreglo de 2^pt2_bits PTEs (se crea bajo demanda).
 */
#ifndef PAGETABLE_H
#define PAGETABLE_H

#include <stdint.h>
#include "config.h"

/* Page Table Entry */
typedef struct {
    uint32_t frame;        /* número de página física (válido si valid = 1)   */
    int32_t  swap_slot;    /* copia en "disco" (swap); -1 si no existe         */
    unsigned valid    : 1; /* 1 = la página está en memoria física             */
    unsigned accessed : 1; /* 1 = fue referenciada desde que se cargó          */
    unsigned dirty    : 1; /* 1 = fue modificada (hay que hacer write-back)    */
} pte_t;

typedef struct {
    pte_t   **l1;          /* tabla de nivel 1                                 */
    uint32_t  n_l1;        /* número de entradas de L1                         */
    uint32_t  n_l2;        /* tablas L2 actualmente existentes (estadística)   */
} pagetable_t;

/* ---- Descomposición de una dirección virtual ---- */
uint32_t pt1_index(const vm_config_t *cfg, uint32_t va);    /* bits 31..22     */
uint32_t pt2_index(const vm_config_t *cfg, uint32_t va);    /* bits 21..12     */
uint32_t page_offset(const vm_config_t *cfg, uint32_t va);  /* bits 11..0      */
uint32_t va_to_vpn(const vm_config_t *cfg, uint32_t va);    /* va >> offset    */

/* ---- Ciclo de vida ---- */
pagetable_t *pt_create(const vm_config_t *cfg);
void         pt_destroy(pagetable_t *pt, const vm_config_t *cfg);

/* Busca la PTE de 'va'. Devuelve NULL si la tabla L2 no existe (NO la crea). */
pte_t *pt_lookup(const pagetable_t *pt, const vm_config_t *cfg, uint32_t va);

/* Igual que pt_lookup pero crea la tabla L2 si no existe. NULL = sin memoria. */
pte_t *pt_get_or_create(pagetable_t *pt, const vm_config_t *cfg, uint32_t va);

/* Libera la tabla L2 número 'i1' si ya no tiene ninguna PTE en uso. */
void pt_release_if_empty(pagetable_t *pt, const vm_config_t *cfg, uint32_t i1);

#endif /* PAGETABLE_H */
