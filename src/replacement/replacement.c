/*
 * replacement.c - LRU sobre una lista doblemente enlazada de frames.
 *
 * Las listas se implementan con arreglos prev[]/next[] indexados por número
 * de frame (no hay malloc por nodo): head = frame más viejo, tail = más nuevo.
 */
#include <stdlib.h>
#include "replacement.h"

struct replacer {
    uint32_t n;          /* número de frames                                */
    int32_t *prev;       /* prev[f]: frame anterior en la lista (-1 = ninguno) */
    int32_t *next;       /* next[f]: frame siguiente (-1 = ninguno)         */
    uint8_t *in_list;    /* 1 si el frame está en la lista                  */
    int32_t  head;       /* candidato a víctima (más viejo)                 */
    int32_t  tail;       /* más recientemente cargado/usado                 */
};

/* ---- Operaciones internas sobre la lista ---- */

static void list_append(replacer_t *r, uint32_t f)
{
    r->prev[f] = r->tail;
    r->next[f] = -1;
    if (r->tail >= 0)
        r->next[r->tail] = (int32_t)f;
    else
        r->head = (int32_t)f;
    r->tail = (int32_t)f;
    r->in_list[f] = 1;
}

static void list_remove(replacer_t *r, uint32_t f)
{
    if (!r->in_list[f])
        return;
    int32_t p = r->prev[f];
    int32_t n = r->next[f];
    if (p >= 0) r->next[p] = n; else r->head = n;
    if (n >= 0) r->prev[n] = p; else r->tail = p;
    r->in_list[f] = 0;
}

/* ---- API pública ---- */

replacer_t *repl_create(uint32_t num_frames)
{
    replacer_t *r = calloc(1, sizeof *r);
    if (!r)
        return NULL;
    r->n       = num_frames;
    r->prev    = calloc(num_frames, sizeof(int32_t));
    r->next    = calloc(num_frames, sizeof(int32_t));
    r->in_list = calloc(num_frames, sizeof(uint8_t));
    r->head = r->tail = -1;
    if (!r->prev || !r->next || !r->in_list) {
        repl_destroy(r);
        return NULL;
    }
    return r;
}

void repl_destroy(replacer_t *r)
{
    if (!r)
        return;
    free(r->prev);
    free(r->next);
    free(r->in_list);
    free(r);
}

void repl_on_load(replacer_t *r, uint32_t frame)
{
    list_remove(r, frame);        /* por seguridad: nunca duplicar un frame */
    list_append(r, frame);        /* entra al final: es el más nuevo        */
}

void repl_on_access(replacer_t *r, uint32_t frame)
{
    if (r->in_list[frame] && r->tail != (int32_t)frame) {
        list_remove(r, frame);
        list_append(r, frame);
    }
}

void repl_on_remove(replacer_t *r, uint32_t frame)
{
    list_remove(r, frame);
}

int32_t repl_select_victim(replacer_t *r)
{
    if (r->head < 0)
        return -1;
    int32_t victim = r->head;     /* el menos recientemente usado */
    list_remove(r, (uint32_t)victim);
    return victim;
}
