/*
 * vmm.c - alloc / free / read / write y estadísticas.
 *
 * alloc reserva un rango de direcciones virtuales (no gasta RAM). Las páginas
 * se traen a memoria física bajo demanda, en el primer acceso (demand paging).
 * Las direcciones virtuales se entregan de forma incremental: el primer alloc
 * empieza en 0, el siguiente justo después, siempre alineado a página.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vmm.h"

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                       */
/* ------------------------------------------------------------------ */

vmm_t *vmm_create(const vm_config_t *cfg)
{
    vmm_t *vm = calloc(1, sizeof *vm);
    if (!vm)
        return NULL;
    vm->cfg  = *cfg;
    vm->pt   = pt_create(&vm->cfg);
    vm->pm   = pm_create(&vm->cfg);
    vm->repl = repl_create(vm->cfg.num_frames);
    if (!vm->pt || !vm->pm || !vm->repl) {
        vmm_destroy(vm);
        return NULL;
    }
    return vm;
}

void vmm_destroy(vmm_t *vm)
{
    if (!vm)
        return;
    pt_destroy(vm->pt, &vm->cfg);
    pm_destroy(vm->pm);
    repl_destroy(vm->repl);
    free(vm->regions);
    free(vm);
}

/* ------------------------------------------------------------------ */
/* Regiones virtuales                                                  */
/* ------------------------------------------------------------------ */

/* ¿'va' cae dentro de alguna región asignada? */
static int region_contains(const vmm_t *vm, uint32_t va)
{
    for (size_t i = 0; i < vm->n_regions; i++) {
        const region_t *r = &vm->regions[i];
        if (va >= r->base && (uint64_t)va < (uint64_t)r->base + r->size)
            return 1;
    }
    return 0;
}

int vmm_alloc(vmm_t *vm, uint64_t bytes, uint32_t *out_va)
{
    uint64_t ps = vm->cfg.page_size;

    if (bytes == 0) {
        fprintf(stderr, "Error: alloc de 0 bytes.\n");
        return -1;
    }
    if (bytes > (1ULL << VA_BITS) ||
        vm->next_va + ((bytes + ps - 1) / ps) * ps > (1ULL << VA_BITS)) {
        fprintf(stderr, "Error: alloc de %" PRIu64 " bytes excede el espacio "
                "virtual de 32 bits.\n", bytes);
        return -1;
    }

    if (vm->n_regions == vm->cap_regions) {
        size_t ncap = vm->cap_regions ? vm->cap_regions * 2 : 8;
        region_t *nr = realloc(vm->regions, ncap * sizeof(region_t));
        if (!nr) {
            fprintf(stderr, "Error: sin memoria.\n");
            return -1;
        }
        vm->regions     = nr;
        vm->cap_regions = ncap;
    }

    region_t r;
    r.base = (uint32_t)vm->next_va;
    r.size = bytes;
    vm->regions[vm->n_regions++] = r;

    /* Avanzar el "break" al siguiente límite de página */
    vm->next_va += ((bytes + ps - 1) / ps) * ps;
    *out_va = r.base;
    return 0;
}

int vmm_free(vmm_t *vm, uint32_t va)
{
    /* free solo acepta la dirección inicial de un alloc previo */
    size_t idx = vm->n_regions;
    for (size_t i = 0; i < vm->n_regions; i++) {
        if (vm->regions[i].base == va) {
            idx = i;
            break;
        }
    }
    if (idx == vm->n_regions) {
        fprintf(stderr, "Error: free(0x%08" PRIx32 ") no es el inicio de una "
                "región asignada.\n", va);
        return -1;
    }

    region_t r = vm->regions[idx];
    uint64_t ps     = vm->cfg.page_size;
    uint64_t npages = (r.size + ps - 1) / ps;

    /* Liberar cada página de la región: frame, copia en swap y PTE */
    for (uint64_t k = 0; k < npages; k++) {
        uint32_t page_va = (uint32_t)(r.base + k * ps);
        pte_t *pte = pt_lookup(vm->pt, &vm->cfg, page_va);
        if (!pte)
            continue;                        /* nunca se tocó: nada que liberar */
        if (pte->valid) {
            repl_on_remove(vm->repl, pte->frame);
            pm_release_frame(vm->pm, pte->frame);
        }
        if (pte->swap_slot >= 0)
            swap_release(vm->pm, pte->swap_slot);
        pte->valid     = 0;
        pte->accessed  = 0;
        pte->dirty     = 0;
        pte->swap_slot = -1;
        pte->frame     = 0;
    }

    /* Liberar las tablas de nivel 2 que quedaron vacías */
    uint32_t last_va = (uint32_t)(r.base + npages * ps - 1);
    for (uint32_t i1 = pt1_index(&vm->cfg, r.base);
         i1 <= pt1_index(&vm->cfg, last_va); i1++)
        pt_release_if_empty(vm->pt, &vm->cfg, i1);

    /* Quitar la región de la lista */
    memmove(&vm->regions[idx], &vm->regions[idx + 1],
            (vm->n_regions - idx - 1) * sizeof(region_t));
    vm->n_regions--;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Lectura / escritura                                                 */
/* ------------------------------------------------------------------ */

/* Acceso común: valida la dirección, traduce y, si hay fallo, lo atiende. */
static int vmm_access(vmm_t *vm, uint32_t va, int is_write, uint32_t *pa)
{
    if (!region_contains(vm, va)) {
        vm->stats.invalid_accesses++;
        fprintf(stderr, "Error: VA 0x%08" PRIx32 " no pertenece a ninguna "
                "región asignada (segfault simulado).\n", va);
        return -1;
    }

    vm->stats.total_accesses++;
    vm->stats.sim_time_ns += T_MEM_NS;
    if (is_write) vm->stats.writes++; else vm->stats.reads++;

    int fault = 0;
    if (translate(vm, va, is_write, pa) == TR_FAULT) {
        fault = 1;
        if (handle_page_fault(vm, va) != 0)
            return -1;
        /* Reintentar: ahora la página está en memoria y debe ser un hit */
        if (translate(vm, va, is_write, pa) != TR_OK) {
            fprintf(stderr, "Error interno: la traducción falló tras el fallo.\n");
            return -1;
        }
    }
    if (vm->cfg.verbose)
        vmm_print_translation(vm, va, *pa, fault, stdout);
    return 0;
}

int vmm_read(vmm_t *vm, uint32_t va, uint8_t *out)
{
    uint32_t pa;
    if (vmm_access(vm, va, 0, &pa) != 0)
        return -1;
    *out = vm->pm->mem[pa];
    return 0;
}

int vmm_write(vmm_t *vm, uint32_t va, uint8_t value)
{
    uint32_t pa;
    if (vmm_access(vm, va, 1, &pa) != 0)
        return -1;
    vm->pm->mem[pa] = value;
    if (vm->cfg.verbose)
        fprintf(stdout, "      Dato guardado en PA %" PRIu32 ": %u\n",
                pa, (unsigned)value);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Estadísticas                                                        */
/* ------------------------------------------------------------------ */

void vmm_print_stats(const vmm_t *vm, FILE *out)
{
    const vm_stats_t *s = &vm->stats;

    fprintf(out, "\n========== Estadísticas finales ==========\n");
    fprintf(out, "Política: LRU\n");
    fprintf(out, "Memoria física: %u KB (%u frames de %u bytes)\n",
            vm->cfg.phys_bytes / 1024u, vm->cfg.num_frames, vm->cfg.page_size);
    fprintf(out, "Total de accesos: %" PRIu64 "\n", s->total_accesses);
    fprintf(out, "Total fallos de página: %" PRIu64 "\n", s->page_faults);
    if (s->total_accesses > 0)
        fprintf(out, "Hit rate: %.2f%%\n",
                100.0 * (double)(s->total_accesses - s->page_faults) /
                (double)s->total_accesses);
    else
        fprintf(out, "Hit rate: N/A (sin accesos)\n");
    fprintf(out, "Total reemplazos: %" PRIu64 "\n", s->replacements);

    fprintf(out, "----------- Detalle adicional -----------\n");
    fprintf(out, "Lecturas / escrituras: %" PRIu64 " / %" PRIu64 "\n",
            s->reads, s->writes);
    fprintf(out, "Lecturas de disco (swap-in): %" PRIu64 "\n", s->disk_reads);
    fprintf(out, "Escrituras a disco (write-back): %" PRIu64 "\n", s->disk_writes);
    fprintf(out, "Accesos inválidos: %" PRIu64 "\n", s->invalid_accesses);
    fprintf(out, "Tablas de nivel 2: %" PRIu64 " creadas, %u activas al final\n",
            s->l2_created, vm->pt->n_l2);
    fprintf(out, "Tiempo simulado: %.3f ms\n", (double)s->sim_time_ns / 1e6);
}

/* ------------------------------------------------------------------ */
/* Trazas de traducción y volcado de la tabla de páginas               */
/* ------------------------------------------------------------------ */

void vmm_print_address_layout(const vm_config_t *cfg, FILE *out)
{
    uint32_t pt1_low = cfg->pt2_bits + cfg->offset_bits;
    uint32_t pt2_low = cfg->offset_bits;

    fprintf(out, "\n========== Formato de dirección virtual ==========\n");
    fprintf(out, "VA (%u bits) = [ PT1: %u bits ] [ PT2: %u bits ] "
         "[ Offset: %u bits ]\n",
        VA_BITS, cfg->pt1_bits, cfg->pt2_bits, cfg->offset_bits);
    fprintf(out, "PT1: bits [%u:%u], índice de la tabla de nivel 1\n",
        VA_BITS - 1u, pt1_low);
    fprintf(out, "PT2: bits [%u:%u], índice de la tabla de nivel 2\n",
        pt1_low - 1u, pt2_low);
    fprintf(out, "Offset: bits [%u:0], posición dentro de la página\n",
        pt2_low - 1u);
    fprintf(out, "==================================================\n");
}

static void print_binary_range(uint32_t value, uint32_t high_bit,
                   uint32_t low_bit, FILE *out)
{
    for (int bit_index = (int)high_bit; bit_index >= (int)low_bit; bit_index--)
    fputc(((value >> bit_index) & 1u) ? '1' : '0', out);
}

void vmm_print_translation(const vmm_t *vm, uint32_t va, uint32_t pa,
                            int fault, FILE *out)
{
    const vm_config_t *cfg = &vm->cfg;
    uint32_t p1  = pt1_index(cfg, va);
    uint32_t p2  = pt2_index(cfg, va);
    uint32_t off = page_offset(cfg, va);
    uint32_t vpn = va_to_vpn(cfg, va);
    uint32_t pfn = pa >> cfg->offset_bits;

        uint32_t pt1_low = cfg->pt2_bits + cfg->offset_bits;
        uint32_t pt2_low = cfg->offset_bits;

        fprintf(out, "    Traducción VA -> PA:\n");
        fprintf(out, "      VA decimal: %" PRIu32 "\n", va);
        fprintf(out, "      VA binaria: ");
        print_binary_range(va, VA_BITS - 1u, pt1_low, out);
        fputs(" | ", out);
        print_binary_range(va, pt1_low - 1u, pt2_low, out);
        fputs(" | ", out);
        print_binary_range(va, pt2_low - 1u, 0, out);
        fputc('\n', out);
        fprintf(out, "      PT1 (bits [%u:%u]) = %" PRIu32
             " -> entrada de tabla de nivel 1\n",
            VA_BITS - 1u, pt1_low, p1);
        fprintf(out, "      PT2 (bits [%u:%u]) = %" PRIu32
             " -> entrada de tabla de nivel 2\n",
            pt1_low - 1u, pt2_low, p2);
        fprintf(out, "      Offset (bits [%u:0]) = %" PRIu32
             " -> byte dentro de la página\n",
            pt2_low - 1u, off);
        fprintf(out, "      VPN %" PRIu32 " -> PFN/frame %" PRIu32 " (%s)\n",
            vpn, pfn, fault ? "página cargada tras fallo" : "página residente");
        fprintf(out, "      PA = frame %" PRIu32 " * tamaño de página %" PRIu32
                     " + offset %" PRIu32 " = %" PRIu32 "\n",
            pfn, cfg->page_size, off, pa);
        fprintf(out, "      PA binaria: ");
        print_binary_range(pa, VA_BITS - 1u, 0, out);
        fputc('\n', out);
}

void vmm_dump_pagetable(const vmm_t *vm, FILE *out)
{
    const vm_config_t *cfg = &vm->cfg;

    uint32_t pt1_low = cfg->pt2_bits + cfg->offset_bits;
    uint32_t pt2_low = cfg->offset_bits;
    fprintf(out, "\n----- Tabla de páginas (PT1.PT2 = VPN) -----\n");
    fprintf(out, "PT1: bits [%u:%u] | PT2: bits [%u:%u] | "
                 "Offset: bits [%u:0]\n",
            VA_BITS - 1u, pt1_low, pt1_low - 1u, pt2_low, pt2_low - 1u);
    fprintf(out, "%-6s %-6s %-10s %-8s %-6s %-6s %-6s %-6s\n",
            "PT1", "PT2", "VPN", "valid", "acc", "dirty", "frame", "swap");

    uint32_t n2 = 1u << cfg->pt2_bits;
    uint32_t shown = 0;
    for (uint32_t i1 = 0; i1 < vm->pt->n_l1; i1++) {
        pte_t *l2 = vm->pt->l1[i1];
        if (!l2)
            continue;
        for (uint32_t i2 = 0; i2 < n2; i2++) {
            pte_t *pte = &l2[i2];
            if (!pte->valid && pte->swap_slot < 0)
                continue;                 /* fila nunca usada: se omite */
            uint32_t vpn = (i1 << cfg->pt2_bits) | i2;
            fprintf(out, "%-6" PRIu32 " %-6" PRIu32 " %-10" PRIu32
                    " %-8d %-6d %-6d ", i1, i2, vpn,
                    pte->valid, pte->accessed, pte->dirty);
            if (pte->valid)
                fprintf(out, "%-6" PRIu32 " ", pte->frame);
            else
                fprintf(out, "%-6s ", "-");
            if (pte->swap_slot >= 0)
                fprintf(out, "%-6" PRId32 "\n", pte->swap_slot);
            else
                fprintf(out, "%-6s\n", "-");
            shown++;
        }
    }
    if (shown == 0)
        fprintf(out, "(vacía: ninguna página ha sido tocada)\n");
    fprintf(out, "---------------------------------------------\n");
}
