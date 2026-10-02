#ifndef STATISTICS_H
#define STATISTICS_H
#include "sim_virtual.h"

void stats_reset(Stats *stats);
void stats_record_access(Stats *stats);
void stats_record_page_fault(Stats *stats);
void stats_record_replacement(Stats *stats);
void stats_record_tlb_hit(Stats *stats);

#endif