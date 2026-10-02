#include "sim_virtual.h"
#include <stdlib.h>
#include <stdio.h>

void frame_table_init(FrameTable *table, uint32_t frame_count) {
    if (table == NULL) {
        return;
    }
    table->frame_count = frame_count;
    table->frames = (FrameInfo *)calloc(frame_count, sizeof(FrameInfo));
    table->free_pool = (uint32_t *)malloc(sizeof(uint32_t) * frame_count);
    table->free_head = 0u;
    table->free_tail = 0u;
    table->free_count = frame_count;
    for (uint32_t i = 0; i < frame_count; ++i) {
        table->free_pool[i] = i;
        table->frames[i].is_allocated = false;
        table->frames[i].vpn = 0u;
        table->frames[i].directory_index = 0u;
        table->frames[i].page_table_index = 0u;
    }
}

void frame_table_destroy(FrameTable *table) {
    if (table == NULL) {
        return;
    }
    free(table->frames);
    free(table->free_pool);
    table->frame_count = 0u;
    table->free_head = 0u;
    table->free_tail = 0u;
    table->free_count = 0u;
}

bool frame_table_has_free_frame(const FrameTable *table) {
    return table != NULL && table->free_count > 0u;
}

uint32_t frame_table_allocate_free_frame(FrameTable *table, uint32_t vpn, uint32_t directory_index, uint32_t page_table_index) {
    if (table == NULL || !frame_table_has_free_frame(table)) {
        fprintf(stderr, "No hay marcos libres\n");
        exit(1);
    }
    const uint32_t frame = table->free_pool[table->free_head];
    table->free_head = (table->free_head + 1u) % table->frame_count;
    table->free_count--;
    frame_table_map_frame(table, frame, vpn, directory_index, page_table_index);
    return frame;
}

void frame_table_map_frame(FrameTable *table, uint32_t frame, uint32_t vpn, uint32_t directory_index, uint32_t page_table_index) {
    if (table == NULL || frame >= table->frame_count) {
        fprintf(stderr, "Frame fuera de rango\n");
        exit(1);
    }
    table->frames[frame].is_allocated = true;
    table->frames[frame].vpn = vpn;
    table->frames[frame].directory_index = directory_index;
    table->frames[frame].page_table_index = page_table_index;
}

void frame_table_free_frame(FrameTable *table, uint32_t frame) {
    if (table == NULL || frame >= table->frame_count) {
        return;
    }
    table->frames[frame].is_allocated = false;
    table->frames[frame].vpn = 0u;
    table->frames[frame].directory_index = 0u;
    table->frames[frame].page_table_index = 0u;
    table->free_pool[table->free_tail] = frame;
    table->free_tail = (table->free_tail + 1u) % table->frame_count;
    table->free_count++;
}

const FrameInfo *frame_table_get_frame_info(const FrameTable *table, uint32_t frame) {
    if (table == NULL || frame >= table->frame_count) {
        return NULL;
    }
    return &table->frames[frame];
}
