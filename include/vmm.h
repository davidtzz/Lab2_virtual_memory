/*
 * vmm.h - Administrador de memoria virtual (Virtual Memory Manager).
 *
 * Une todas las piezas: tabla de páginas, memoria física, política de
 * reemplazo y estadísticas. Es la API que usa el intérprete de comandos.
 *
 *   translate.c  -> traducción VA -> PA
 *   pagefault.c  -> manejo de fallos de página (incluye el reemplazo)
 *   vmm.c        -> alloc / free / read / write / estadísticas
 */
#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "config.h"
#include "pagetable.h"
#include "physmem.h"
#include "replacement.h"

/* Región virtual reservada con alloc */
typedef struct {
    uint32_t base;   /* dirección virtual inicial (alineada a página)          */
    uint64_t size;   /* bytes solicitados                                       */
} region_t;

typedef struct {
    uint64_t total_accesses;    /* lecturas + escrituras válidas               */
    uint64_t reads;
    uint64_t writes;
    uint64_t page_faults;
    uint64_t replacements;      /* fallos que obligaron a expulsar una página  */
    uint64_t disk_reads;        /* páginas traídas desde swap                  */
    uint64_t disk_writes;       /* write-backs de páginas sucias               */
    uint64_t invalid_accesses;  /* accesos a direcciones no asignadas          */
    uint64_t l2_created;        /* tablas de nivel 2 creadas en total          */
    uint64_t sim_time_ns;       /* tiempo simulado                             */
} vm_stats_t;

typedef struct vmm {
    vm_config_t  cfg;
    pagetable_t *pt;
    physmem_t   *pm;
    replacer_t  *repl;
    vm_stats_t   stats;
    region_t    *regions;       /* regiones vivas (alloc sin free)             */
    size_t       n_regions;
    size_t       cap_regions;
    uint64_t     next_va;       /* siguiente dirección virtual libre (bump)    */
} vmm_t;

typedef enum { TR_OK = 0, TR_FAULT = 1 } tr_result_t;

/* ---- vmm.c ---- */
vmm_t *vmm_create(const vm_config_t *cfg);
void   vmm_destroy(vmm_t *vm);
int    vmm_alloc(vmm_t *vm, uint64_t bytes, uint32_t *out_va);
int    vmm_free(vmm_t *vm, uint32_t va);
int    vmm_read(vmm_t *vm, uint32_t va, uint8_t *out);
int    vmm_write(vmm_t *vm, uint32_t va, uint8_t value);
void   vmm_print_stats(const vmm_t *vm, FILE *out);
void   vmm_print_address_layout(const vm_config_t *cfg, FILE *out);

/* Imprime la descomposición de una VA (PT1/PT2/offset/VPN) y su traducción
 * (frame/PA). 'fault' indica si el acceso disparó un fallo de página, solo
 * para la etiqueta que se muestra. */
void   vmm_print_translation(const vmm_t *vm, uint32_t va, uint32_t pa,
                              int fault, FILE *out);

/* Imprime la tabla de páginas completa (solo las PTEs en uso: valid=1 o con
 * copia en swap) como una tabla de texto: VPN, PT1, PT2, frame/swap, bits. */
void   vmm_dump_pagetable(const vmm_t *vm, FILE *out);

/* ---- translate.c ---- */
/* Traduce VA -> PA. Si la página no está en RAM devuelve TR_FAULT. */
tr_result_t translate(vmm_t *vm, uint32_t va, int is_write, uint32_t *pa);

/* ---- pagefault.c ---- */
/* Atiende un fallo de página: consigue un frame (reemplazando si hace falta),
 * carga la página y actualiza la PTE. 0 = ok, -1 = error interno. */
int handle_page_fault(vmm_t *vm, uint32_t va);

#endif /* VMM_H */
