/*
 * replacement.h - Reemplazo de páginas LRU.
 *
 * La lista doblemente enlazada ordena los frames desde el menos recientemente
 * usado (head) hasta el más recientemente usado (tail). La víctima es el head.
 * Todas las operaciones son O(1).
 */
#ifndef REPLACEMENT_H
#define REPLACEMENT_H

#include <stdint.h>
#include "config.h"

typedef struct replacer replacer_t;

replacer_t *repl_create(uint32_t num_frames);
void        repl_destroy(replacer_t *r);

/* Una página acaba de cargarse en 'frame' (entra como la más nueva). */
void    repl_on_load(replacer_t *r, uint32_t frame);
/* Hubo un acceso (hit) a la página que está en 'frame'. */
void    repl_on_access(replacer_t *r, uint32_t frame);
/* El frame se liberó por un free(): sacarlo de la lista. */
void    repl_on_remove(replacer_t *r, uint32_t frame);
/* Elige y SACA de la lista al frame víctima. Devuelve -1 si la lista está vacía. */
int32_t repl_select_victim(replacer_t *r);

#endif /* REPLACEMENT_H */
