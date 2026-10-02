#include "sim_virtual.h"
#include <stdio.h>

void stats_reset(Stats *stats) {
    if (stats == NULL) {
        return;
    }
    stats->total_accesses = 0u;
    stats->page_faults = 0u;
    stats->replacements = 0u;
    stats->tlb_hits = 0u;
    stats->elapsed_ticks = 0u;
}

void stats_record_access(Stats *stats) {
    if (stats == NULL) {
        return;
    }
    stats->total_accesses++;
}

void stats_record_page_fault(Stats *stats) {
    if (stats == NULL) {
        return;
    }
    stats->page_faults++;
}

void stats_record_replacement(Stats *stats) {
    if (stats == NULL) {
        return;
    }
    stats->replacements++;
}

void stats_record_tlb_hit(Stats *stats) {
    if (stats == NULL) {
        return;
    }
    stats->tlb_hits++;
}


