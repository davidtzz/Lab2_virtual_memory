/*
 * physmem.h - Memoria física simulada (frames) y área de swap ("disco").
 *
 * La memoria física es un arreglo de bytes real, dividido en num_frames
 * frames del tamaño de página. El swap guarda las páginas modificadas que
 * fueron expulsadas, para no perder sus datos cuando se vuelvan a usar.
 */
#ifndef PHYSMEM_H
#define PHYSMEM_H

#include <stdint.h>
#include "config.h"

/* Información por frame (tabla inversa: quién es el dueño del frame) */
typedef struct {
    uint8_t  used;   /* 1 = ocupado                                         */
    uint32_t vpn;    /* número de página virtual que contiene               */
} frame_info_t;

typedef struct {
    uint8_t      *mem;          /* bytes de RAM: num_frames * page_size      */
    uint32_t      page_size;
    uint32_t      num_frames;
    frame_info_t *frames;       /* metadatos por frame                       */
    uint32_t     *free_frames;  /* pila de frames libres                     */
    uint32_t      n_free_frames;

    uint8_t     **swap_slots;   /* cada slot guarda una página               */
    uint32_t     *swap_free;    /* pila de slots reutilizables               */
    uint32_t      swap_n;       /* slots creados                             */
    uint32_t      swap_cap;     /* capacidad de los arreglos anteriores      */
    uint32_t      swap_nfree;   /* slots libres disponibles                  */
} physmem_t;

physmem_t *pm_create(const vm_config_t *cfg);
void       pm_destroy(physmem_t *pm);

/* Toma un frame libre y lo asigna a 'vpn'. Devuelve -1 si no quedan libres. */
int32_t  pm_alloc_frame(physmem_t *pm, uint32_t vpn);
/* Devuelve un frame a la lista de libres. */
void     pm_release_frame(physmem_t *pm, uint32_t frame);
/* Puntero al inicio de los bytes del frame. */
uint8_t *pm_frame_ptr(const physmem_t *pm, uint32_t frame);

/* ---- Swap ---- */
int32_t  swap_alloc(physmem_t *pm);                   /* -1 si no hay memoria */
void     swap_release(physmem_t *pm, int32_t slot);
uint8_t *swap_ptr(const physmem_t *pm, int32_t slot);

#endif /* PHYSMEM_H */
