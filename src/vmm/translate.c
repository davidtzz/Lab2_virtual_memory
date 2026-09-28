/*
 * translate.c - Traducción de direcciones virtuales a físicas (VA -> PA).
 *
 * Recorrido de la tabla de dos niveles:
 *
 *   VA = | PT1 (10b) | PT2 (10b) | Offset (12b) |
 *          |            |
 *          |            +--> índice en la tabla L2  -> PTE
 *          +---------------> índice en la tabla L1  -> puntero a L2
 *
 *   PA = (PTE.frame << offset_bits) | offset
 */
#include "vmm.h"

tr_result_t translate(vmm_t *vm, uint32_t va, int is_write, uint32_t *pa)
{
    /* Paso 1-2: L1 -> L2 -> PTE. Si la tabla L2 no existe, es un fallo. */
    pte_t *pte = pt_lookup(vm->pt, &vm->cfg, va);

    /* Paso 3: PTE inexistente o con valid = 0 -> fallo de página */
    if (pte == NULL || !pte->valid)
        return TR_FAULT;

    /* Paso 4: la página está en RAM (hit). Actualizar bits de estado. */
    pte->accessed = 1;
    if (is_write)
        pte->dirty = 1;

    /* LRU marca este frame como el más recientemente usado. */
    repl_on_access(vm->repl, pte->frame);

    /* Paso 5: armar la dirección física */
    *pa = (pte->frame << vm->cfg.offset_bits) | page_offset(&vm->cfg, va);
    return TR_OK;
}
