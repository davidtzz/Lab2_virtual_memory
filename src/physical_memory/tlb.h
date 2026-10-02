#ifndef TLB_H
#define TLB_H
#include "sim_virtual.h"

void tlb_init(TLB *tlb);
bool tlb_lookup(const TLB *tlb, uint32_t vpn, uint32_t *frame_out);
void tlb_insert(TLB *tlb, uint32_t vpn, uint32_t frame, uint64_t now_tick);
void tlb_invalidate_vpn(TLB *tlb, uint32_t vpn);

#endif
