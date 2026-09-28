/*
 * physmem.c - Memoria física (frames) y swap simulado.
 */
#include <stdlib.h>
#include "physmem.h"

physmem_t *pm_create(const vm_config_t *cfg)
{
    physmem_t *pm = calloc(1, sizeof *pm);
    if (!pm)
        return NULL;

    pm->page_size  = cfg->page_size;
    pm->num_frames = cfg->num_frames;
    pm->mem         = calloc(cfg->num_frames, cfg->page_size);
    pm->frames      = calloc(cfg->num_frames, sizeof(frame_info_t));
    pm->free_frames = malloc(cfg->num_frames * sizeof(uint32_t));
    if (!pm->mem || !pm->frames || !pm->free_frames) {
        pm_destroy(pm);
        return NULL;
    }

    /* Se apila en orden inverso para que el primer frame entregado sea el 0
     * (así la salida es determinista y fácil de seguir). */
    for (uint32_t i = 0; i < cfg->num_frames; i++)
        pm->free_frames[i] = cfg->num_frames - 1u - i;
    pm->n_free_frames = cfg->num_frames;
    return pm;
}

void pm_destroy(physmem_t *pm)
{
    if (!pm)
        return;
    for (uint32_t i = 0; i < pm->swap_n; i++)
        free(pm->swap_slots[i]);
    free(pm->swap_slots);
    free(pm->swap_free);
    free(pm->free_frames);
    free(pm->frames);
    free(pm->mem);
    free(pm);
}

/* ------------------------------------------------------------------ */
/* Frames                                                              */
/* ------------------------------------------------------------------ */

int32_t pm_alloc_frame(physmem_t *pm, uint32_t vpn)
{
    if (pm->n_free_frames == 0)
        return -1;
    uint32_t f = pm->free_frames[--pm->n_free_frames];
    pm->frames[f].used = 1;
    pm->frames[f].vpn  = vpn;
    return (int32_t)f;
}

void pm_release_frame(physmem_t *pm, uint32_t frame)
{
    pm->frames[frame].used = 0;
    pm->free_frames[pm->n_free_frames++] = frame;
}

uint8_t *pm_frame_ptr(const physmem_t *pm, uint32_t frame)
{
    return pm->mem + (size_t)frame * pm->page_size;
}

/* ------------------------------------------------------------------ */
/* Swap                                                                */
/* ------------------------------------------------------------------ */

int32_t swap_alloc(physmem_t *pm)
{
    /* 1) Reutilizar un slot liberado antes */
    if (pm->swap_nfree > 0)
        return (int32_t)pm->swap_free[--pm->swap_nfree];

    /* 2) Crecer los arreglos si hace falta (capacidad doble) */
    if (pm->swap_n == pm->swap_cap) {
        uint32_t ncap = pm->swap_cap ? pm->swap_cap * 2u : 64u;
        uint8_t **ns = realloc(pm->swap_slots, ncap * sizeof(uint8_t *));
        if (!ns)
            return -1;
        pm->swap_slots = ns;
        uint32_t *nf = realloc(pm->swap_free, ncap * sizeof(uint32_t));
        if (!nf)
            return -1;
        pm->swap_free = nf;
        pm->swap_cap  = ncap;
    }

    /* 3) Crear un slot nuevo (una página de almacenamiento) */
    uint8_t *buf = malloc(pm->page_size);
    if (!buf)
        return -1;
    pm->swap_slots[pm->swap_n] = buf;
    return (int32_t)pm->swap_n++;
}

void swap_release(physmem_t *pm, int32_t slot)
{
    pm->swap_free[pm->swap_nfree++] = (uint32_t)slot;
}

uint8_t *swap_ptr(const physmem_t *pm, int32_t slot)
{
    return pm->swap_slots[slot];
}
