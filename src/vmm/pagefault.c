/*
 * pagefault.c - Manejo de fallos de página y expulsión de páginas.
 *
 * Flujo de handle_page_fault():
 *   1. Contar el fallo y crear la tabla L2 si no existe.
 *   2. Pedir un frame libre. Si no hay, expulsar una víctima según la política
 *      según LRU; si la víctima está sucia, escribirla al swap (write-back).
 *   3. Llenar el frame: desde swap si la página ya había sido expulsada, o con
 *      ceros si es la primera vez que se toca (demand-zero paging).
 *   4. Marcar la PTE como válida y avisar a la política.
 */
#include <stdio.h>
#include <string.h>
#include "vmm.h"

/*
 * Expulsa una página de la memoria física según la política de reemplazo.
 * Devuelve en *frame_out el frame que quedó libre para reutilizar.
 */
static int evict_victim(vmm_t *vm, uint32_t *frame_out)
{
    /* La política decide quién sale */
    int32_t f = repl_select_victim(vm->repl);
    if (f < 0)
        return -1;

    /* Tabla inversa: ¿qué página virtual vive en ese frame? */
    uint32_t vpn       = vm->pm->frames[f].vpn;
    uint32_t victim_va = vpn << vm->cfg.offset_bits;
    pte_t   *vpte      = pt_lookup(vm->pt, &vm->cfg, victim_va);

    if (!vpte || !vpte->valid || vpte->frame != (uint32_t)f) {
        fprintf(stderr, "Error interno: inconsistencia en el frame %d\n", f);
        return -1;
    }

    /* Write-back: solo si la página fue modificada. Si está limpia, o nunca se
     * escribió (sigue siendo todo ceros), basta con descartarla. */
    if (vpte->dirty) {
        if (vpte->swap_slot < 0) {
            vpte->swap_slot = swap_alloc(vm->pm);
            if (vpte->swap_slot < 0)
                return -1;
        }
        memcpy(swap_ptr(vm->pm, vpte->swap_slot),
               pm_frame_ptr(vm->pm, (uint32_t)f), vm->cfg.page_size);
        vm->stats.disk_writes++;
        vm->stats.sim_time_ns += T_DISK_NS;
    }

    /* La página víctima deja de estar en memoria */
    vpte->valid    = 0;
    vpte->accessed = 0;
    vpte->dirty    = 0;
    vm->stats.replacements++;

    if (vm->cfg.verbose)
        printf("  [REEMPLAZO] sale VPN %u (frame %d, %s)\n", vpn, f,
               vpte->swap_slot >= 0 ? "guardada en swap" : "descartada");

    *frame_out = (uint32_t)f;
    return 0;
}

int handle_page_fault(vmm_t *vm, uint32_t va)
{
    vm->stats.page_faults++;
    vm->stats.sim_time_ns += T_FAULT_NS;

    /* 1. Obtener la PTE; se crea la tabla de nivel 2 si aún no existe */
    uint32_t l2_before = vm->pt->n_l2;
    pte_t *pte = pt_get_or_create(vm->pt, &vm->cfg, va);
    if (!pte)
        return -1;
    if (vm->pt->n_l2 > l2_before)
        vm->stats.l2_created++;

    uint32_t vpn = va_to_vpn(&vm->cfg, va);

    /* 2. Conseguir un frame: libre, o expulsando una víctima */
    int32_t f = pm_alloc_frame(vm->pm, vpn);
    if (f < 0) {
        uint32_t victim;
        if (evict_victim(vm, &victim) != 0)
            return -1;
        f = (int32_t)victim;
        vm->pm->frames[f].vpn = vpn;      /* el frame cambia de dueño */
    }

    /* 3. Llenar el frame */
    uint8_t *dst = pm_frame_ptr(vm->pm, (uint32_t)f);
    if (pte->swap_slot >= 0) {
        memcpy(dst, swap_ptr(vm->pm, pte->swap_slot), vm->cfg.page_size);
        vm->stats.disk_reads++;
        vm->stats.sim_time_ns += T_DISK_NS;
    } else {
        memset(dst, 0, vm->cfg.page_size);   /* primera vez: página en ceros */
    }

    /* 4. Actualizar la PTE y notificar a la política */
    pte->frame    = (uint32_t)f;
    pte->valid    = 1;
    pte->accessed = 0;
    pte->dirty    = 0;
    repl_on_load(vm->repl, (uint32_t)f);

    if (vm->cfg.verbose)
        printf("  [FALLO] VPN %u -> frame %d\n", vpn, f);
    return 0;
}
