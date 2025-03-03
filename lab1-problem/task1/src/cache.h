/************************************/
/*                                  */
/*      Cache Implementation        */
/*                                  */
/************************************/

#ifndef _CACHE_H
#define _CACHE_H

#include <stdint.h>
#include <stdbool.h>

/* Instruction cache */
#define I_WAYS 4
#define I_SETS 64
#define I_BLOCK_SIZE 32

/* Data cache */
#define D_WAYS 8
#define D_SETS 256
#define D_BLOCK_SIZE 32

// structure to hold cache metadata
typedef struct{
    uint32_t block_size;
    uint32_t ways;
    uint32_t sets;
} cache_mdata;

// structure to hold a cache_block
typedef struct{
    bool valid;
    bool dirty;
    uint32_t tag;
    uint8_t lru;
    uint32_t* value;
    uint32_t addr;
} cache_block;

// structure to hold a cache_line
// Can have multiple blocks
typedef struct{
    cache_block* way;
} cache_line;

// structure to hold a unified cache unit
// can hold multiple cache lines
typedef struct{
    cache_mdata mdata;
    cache_line* set;
} cache_unit;

extern cache_unit* icache;
extern cache_unit* dcache;

// Member functions
cache_unit* init_cache(uint32_t, uint32_t, uint32_t);
void fill_block(cache_unit*, uint32_t, int, uint32_t);
void evict_block(cache_unit*, uint32_t, int);
uint32_t cache_read(cache_unit*, uint32_t);
void cache_write(cache_unit*, uint32_t, uint32_t);
void evict_block(cache_unit*, uint32_t, int);

// Utility function - clog2
uint32_t clog2(uint32_t);
#endif