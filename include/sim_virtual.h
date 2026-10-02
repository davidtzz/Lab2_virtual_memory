#ifndef SIM_VIRTUAL_H
#define SIM_VIRTUAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SIM_VIRTUAL_ADDRESS_BITS 32u
#define SIM_TLB_SIZE 16u
#define SIM_MIN_PAGE_SIZE 1024u
#define SIM_MAX_PAGE_SIZE 1048576u
#define SIM_MIN_PHYSICAL_MEMORY_SIZE 262144u
#define SIM_MAX_PHYSICAL_MEMORY_SIZE 67108864u

typedef enum {
    SIM_POLICY_FIFO,
    SIM_POLICY_LRU
} SimPolicy;

typedef enum {
    SIM_INSTR_ALLOC,
    SIM_INSTR_WRITE,
    SIM_INSTR_READ,
    SIM_INSTR_FREE
} SimInstructionType;

typedef struct {
    uint32_t page_size;
    uint32_t physical_memory_size;
    uint32_t frame_count;
    uint32_t offset_bits;
    uint32_t directory_index_bits;
    uint32_t page_table_index_bits;
    uint32_t directory_entry_count;
    uint32_t page_table_entry_count;
} MemoryConfig;

typedef struct {
    uint32_t raw;
    uint32_t directory_index;
    uint32_t page_table_index;
    uint32_t offset;
    uint32_t vpn;
} VirtualAddress;

typedef struct {
    uint32_t pfn;
    bool valid_bit;
    bool accessed_bit;
    bool dirty_bit;
    bool allocated_bit;
} PageTableEntry;

typedef struct {
    PageTableEntry *entries;
    uint32_t count;
} PageTable;

typedef struct {
    bool has_table;
    PageTable *table;
} DirectoryEntry;

typedef struct {
    DirectoryEntry *entries;
    uint32_t count;
} DirectoryTable;

typedef struct {
    bool is_allocated;
    uint32_t vpn;
    uint32_t directory_index;
    uint32_t page_table_index;
} FrameInfo;

typedef struct {
    uint32_t frame_count;
    FrameInfo *frames;
    uint32_t *free_pool;
    size_t free_head;
    size_t free_tail;
    size_t free_count;
} FrameTable;

typedef struct {
    uint32_t size;
    uint8_t *storage;
} PhysicalMemory;

typedef struct {
    bool valid;
    uint32_t vpn;
    uint32_t frame;
    uint64_t last_access;
} TLBEntry;

typedef struct {
    TLBEntry entries[SIM_TLB_SIZE];
    uint32_t size;
} TLB;

typedef struct {
    uint64_t total_accesses;
    uint64_t page_faults;
    uint64_t replacements;
    uint64_t tlb_hits;
    uint64_t elapsed_ticks;
} Stats;

typedef struct {
    uint32_t operand;
    uint8_t value;
    SimInstructionType type;
} Instruction;

typedef struct {
    char *file_name;
    uint32_t page_size;
    uint32_t physical_memory_size;
    SimPolicy policy;
} SimulationConfig;

typedef struct {
    MemoryConfig config;
    DirectoryTable directory;
    PhysicalMemory physical_memory;
    FrameTable frame_table;
    TLB tlb;
    uint64_t next_virtual_address;
    Stats stats;
    uint64_t clock;
    SimPolicy policy;
} MemoryManager;

MemoryConfig memory_config_create(uint32_t page_size, uint32_t physical_memory_size);
bool memory_config_is_valid(uint32_t page_size, uint32_t physical_memory_size);
uint32_t memory_config_compute_offset_bits(uint32_t page_size);
uint32_t memory_config_compute_directory_index_bits(uint32_t vpn_bits);

VirtualAddress virtual_address_create(uint32_t raw, const MemoryConfig *config);
uint32_t virtual_address_get_directory_index(const VirtualAddress *va, const MemoryConfig *config);
uint32_t virtual_address_get_page_table_index(const VirtualAddress *va, const MemoryConfig *config);
uint32_t virtual_address_get_offset(const VirtualAddress *va, const MemoryConfig *config);
uint32_t virtual_address_get_vpn(const VirtualAddress *va, const MemoryConfig *config);

void page_table_entry_init(PageTableEntry *entry);
void page_table_entry_allocate(PageTableEntry *entry);
void page_table_entry_load(PageTableEntry *entry, uint32_t pfn);
void page_table_entry_record_access(PageTableEntry *entry, bool is_write);
void page_table_entry_invalidate(PageTableEntry *entry);

PageTable *page_table_create(uint32_t entry_count);
void page_table_destroy(PageTable *table);
PageTableEntry *page_table_get_entry(PageTable *table, uint32_t index);

DirectoryTable directory_table_create(uint32_t entry_count);
void directory_table_destroy(DirectoryTable *directory);
PageTable *directory_table_get_or_create_page_table(DirectoryTable *directory, uint32_t directory_index, uint32_t page_table_entry_count);
PageTable *directory_table_get_page_table(const DirectoryTable *directory, uint32_t directory_index);

void frame_table_init(FrameTable *table, uint32_t frame_count);
void frame_table_destroy(FrameTable *table);
bool frame_table_has_free_frame(const FrameTable *table);
uint32_t frame_table_allocate_free_frame(FrameTable *table, uint32_t vpn, uint32_t directory_index, uint32_t page_table_index);
void frame_table_map_frame(FrameTable *table, uint32_t frame, uint32_t vpn, uint32_t directory_index, uint32_t page_table_index);
void frame_table_free_frame(FrameTable *table, uint32_t frame);
const FrameInfo *frame_table_get_frame_info(const FrameTable *table, uint32_t frame);

void physical_memory_init(PhysicalMemory *memory, uint32_t size);
void physical_memory_destroy(PhysicalMemory *memory);
uint8_t physical_memory_read_byte(const PhysicalMemory *memory, uint32_t physical_address);
void physical_memory_write_byte(PhysicalMemory *memory, uint32_t physical_address, uint8_t value);
void physical_memory_clear_range(PhysicalMemory *memory, uint32_t start_address, uint32_t length);

void tlb_init(TLB *tlb);
bool tlb_lookup(const TLB *tlb, uint32_t vpn, uint32_t *frame_out);
void tlb_insert(TLB *tlb, uint32_t vpn, uint32_t frame, uint64_t now_tick);
void tlb_invalidate_vpn(TLB *tlb, uint32_t vpn);

void stats_reset(Stats *stats);
void stats_record_access(Stats *stats);
void stats_record_page_fault(Stats *stats);
void stats_record_replacement(Stats *stats);
void stats_record_tlb_hit(Stats *stats);

MemoryManager memory_manager_create(const MemoryConfig *config, SimPolicy policy);
void memory_manager_destroy(MemoryManager *manager);
uint32_t memory_manager_allocate(MemoryManager *manager, uint32_t bytes);
void memory_manager_write(MemoryManager *manager, uint32_t virtual_address, uint8_t value);
uint8_t memory_manager_read(MemoryManager *manager, uint32_t virtual_address);
void memory_manager_free(MemoryManager *manager, uint32_t virtual_address);

bool sim_parse_instruction_line(const char *line, Instruction *out_instruction);
bool sim_load_program(const char *file_name, Instruction **instructions_out, size_t *count_out);
void sim_free_program(Instruction *instructions);

void simulation_config_default(SimulationConfig *config);
void simulation_config_print_usage(const char *program_name);

#endif
