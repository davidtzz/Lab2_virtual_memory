#include "sim_virtual.h"
#include <stdio.h>

void tlb_init(TLB *tlb) {
    if (tlb == NULL) {
        return;
    }
    tlb->size = 0u;
    for (uint32_t i = 0; i < SIM_TLB_SIZE; ++i) {
        tlb->entries[i].valid = false;
        tlb->entries[i].vpn = 0u;
        tlb->entries[i].frame = 0u;
        tlb->entries[i].last_access = 0u;
    }
}

bool tlb_lookup(const TLB *tlb, uint32_t vpn, uint32_t *frame_out) {
    if (tlb == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < SIM_TLB_SIZE; ++i) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            if (frame_out != NULL) {
                *frame_out = tlb->entries[i].frame;
            }
            return true;
        }
    }
    return false;
}

void tlb_insert(TLB *tlb, uint32_t vpn, uint32_t frame, uint64_t now_tick) {
    if (tlb == NULL) {
        return;
    }
    int victim_index = -1;
    uint64_t oldest = UINT64_MAX;
    for (uint32_t i = 0; i < SIM_TLB_SIZE; ++i) {
        if (!tlb->entries[i].valid) {
            victim_index = (int)i;
            break;
        }
        if (tlb->entries[i].last_access < oldest) {
            oldest = tlb->entries[i].last_access;
            victim_index = (int)i;
        }
    }
    if (victim_index < 0) {
        victim_index = 0;
    }
    tlb->entries[victim_index].vpn = vpn;
    tlb->entries[victim_index].frame = frame;
    tlb->entries[victim_index].valid = true;
    tlb->entries[victim_index].last_access = now_tick;
    if (tlb->size < SIM_TLB_SIZE) {
        tlb->size++;
    }
}

void tlb_invalidate_vpn(TLB *tlb, uint32_t vpn) {
    if (tlb == NULL) {
        return;
    }
    for (uint32_t i = 0; i < SIM_TLB_SIZE; ++i) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            tlb->entries[i].valid = false;
            tlb->entries[i].vpn = 0u;
            tlb->entries[i].frame = 0u;
            if (tlb->size > 0u) {
                tlb->size--;
            }
            return;
        }
    }
}


