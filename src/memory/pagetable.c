/*
 * pagetable.c - Tabla de páginas de dos niveles (creación bajo demanda).
 */
#include <stdlib.h>
#include "pagetable.h"

/* ------------------------------------------------------------------ */
/* Descomposición de la dirección virtual                              */
/* ------------------------------------------------------------------ */

uint32_t pt1_index(const vm_config_t *cfg, uint32_t va)
{
    return va >> (cfg->offset_bits + cfg->pt2_bits);
}

uint32_t pt2_index(const vm_config_t *cfg, uint32_t va)
{
    return (va >> cfg->offset_bits) & ((1u << cfg->pt2_bits) - 1u);
}

uint32_t page_offset(const vm_config_t *cfg, uint32_t va)
{
    return va & (cfg->page_size - 1u);
}

uint32_t va_to_vpn(const vm_config_t *cfg, uint32_t va)
{
    return va >> cfg->offset_bits;
}

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                       */
/* ------------------------------------------------------------------ */

pagetable_t *pt_create(const vm_config_t *cfg)
{
    pagetable_t *pt = calloc(1, sizeof *pt);
    if (!pt)
        return NULL;
    pt->n_l1 = 1u << cfg->pt1_bits;
    pt->l1   = calloc(pt->n_l1, sizeof(pte_t *));   /* todo NULL: sin L2 */
    if (!pt->l1) {
        free(pt);
        return NULL;
    }
    return pt;
}

void pt_destroy(pagetable_t *pt, const vm_config_t *cfg)
{
    (void)cfg;
    if (!pt)
        return;
    for (uint32_t i = 0; i < pt->n_l1; i++)
        free(pt->l1[i]);
    free(pt->l1);
    free(pt);
}

/* ------------------------------------------------------------------ */
/* Búsqueda / creación de PTEs                                         */
/* ------------------------------------------------------------------ */

pte_t *pt_lookup(const pagetable_t *pt, const vm_config_t *cfg, uint32_t va)
{
    pte_t *l2 = pt->l1[pt1_index(cfg, va)];
    if (!l2)
        return NULL;                 /* la tabla de nivel 2 no existe */
    return &l2[pt2_index(cfg, va)];
}

pte_t *pt_get_or_create(pagetable_t *pt, const vm_config_t *cfg, uint32_t va)
{
    uint32_t i1 = pt1_index(cfg, va);

    if (!pt->l1[i1]) {
        uint32_t n2 = 1u << cfg->pt2_bits;
        pte_t *l2 = calloc(n2, sizeof(pte_t));   /* valid=accessed=dirty=0 */
        if (!l2)
            return NULL;
        for (uint32_t i = 0; i < n2; i++)
            l2[i].swap_slot = -1;                /* sin copia en swap      */
        pt->l1[i1] = l2;
        pt->n_l2++;
    }
    return &pt->l1[i1][pt2_index(cfg, va)];
}

void pt_release_if_empty(pagetable_t *pt, const vm_config_t *cfg, uint32_t i1)
{
    pte_t *l2 = pt->l1[i1];
    if (!l2)
        return;

    uint32_t n2 = 1u << cfg->pt2_bits;
    for (uint32_t i = 0; i < n2; i++) {
        /* Una PTE está "en uso" si la página está en RAM o tiene copia en swap */
        if (l2[i].valid || l2[i].swap_slot >= 0)
            return;
    }
    free(l2);
    pt->l1[i1] = NULL;
    pt->n_l2--;
}
