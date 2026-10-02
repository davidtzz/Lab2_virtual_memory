#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H
#include "sim_virtual.h"

void frame_table_init(FrameTable *table, uint32_t frame_count);
void frame_table_destroy(FrameTable *table);
bool frame_table_has_free_frame(const FrameTable *table);
uint32_t frame_table_allocate_free_frame(FrameTable *table, uint32_t vpn, uint32_t directory_index, uint32_t page_table_index);
void frame_table_map_frame(FrameTable *table, uint32_t frame, uint32_t vpn, uint32_t directory_index, uint32_t page_table_index);
void frame_table_free_frame(FrameTable *table, uint32_t frame);
const FrameInfo *frame_table_get_frame_info(const FrameTable *table, uint32_t frame);


#endif