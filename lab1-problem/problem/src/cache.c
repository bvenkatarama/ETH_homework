#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "cache.h"
#include "shell.h"
#include "pipe.h"

// Allocate and initialize cache
cache_unit* init_cache(uint32_t block_size, uint32_t ways, uint32_t sets){
    cache_unit* cache = malloc(sizeof(cache_unit));
    cache->set = malloc(sets * sizeof(cache_line));

    for(int i=0; i<sets; i++){
        cache->set[i].way = malloc(ways * sizeof(cache_block));
        for(int j=0; j<ways; j++){
            // Each way
            cache->set[i].way[j].valid = false;
            cache->set[i].way[j].dirty = false;
            cache->set[i].way[j].tag = 0;
            cache->set[i].way[j].lru = j;
            cache->set[i].way[j].value = malloc(block_size * sizeof(uint32_t));
            for(int k=0; k<block_size; k++)
                cache->set[i].way[j].value[k] = 0;            
        }
    }

    cache->mdata.block_size = block_size;
    cache->mdata.ways = ways;
    cache->mdata.sets = sets;
    cache->mdata.cache_miss = false;

    return cache;
}

void evict_block(cache_unit* cache, uint32_t idx, int w){
    cache_block* block = &cache->set[idx].way[w];

    // Don't write back into memory if not dirty
    if(block->dirty == false)
        return;
    
    // Evicted block populated back into memory
    else{
        printf("Not allowed to enter here!\n");
        uint32_t sets = cache->mdata.sets;
        uint32_t block_size = cache->mdata.block_size;
        uint32_t tag = block->tag;
        uint32_t evict_addr = (tag<<clog2(sets+block_size)) | (idx<<clog2(block_size));

        for(int m=evict_addr; m<(evict_addr + block_size); ){
            mem_write_32(m, block->value[m-evict_addr]);
            m+=4;
        }
    }
}

void fill_block(cache_unit* cache, uint32_t idx, int w, uint32_t mem_addr){
    uint32_t block_size = cache->mdata.block_size;

    for(int m=mem_addr; m< (mem_addr + block_size); ){
        cache->set[idx].way[w].value[m - mem_addr] = mem_read_32(m);
        m += 4; // address increment in multiples of 4-bytes
    }
}

uint32_t cache_read(cache_unit* cache, uint32_t addr){
    // Meta-data values
    uint32_t block_size = cache->mdata.block_size;
    uint32_t ways = cache->mdata.ways;
    uint32_t sets = cache->mdata.sets;

    // cache-index calculation
    uint32_t idx = (addr>>clog2(block_size)) & (sets-1);    //0x3F = 63 (#sets - 1)
    uint32_t mem_addr = addr & (-1U<<clog2(block_size));    //0x1F = 31 (block_size-1)
    uint32_t tag = addr>>(clog2(sets)+clog2(block_size));
    uint32_t offset = addr & (block_size-1);

    bool rd_done = false;
    uint8_t evict_way=0, max_lru=0;
    uint32_t read_data = 0;
    // bool cache_miss = false;

    // Stall in case of cache miss
    if(pipe.icache_stall > 0){
        printf("Stalling the icache mem access! stall = %u\n", pipe.icache_stall);
        pipe.icache_stall--;
        return 0;
    }

    else if(cache->mdata.cache_miss == true){
        cache->mdata.cache_miss = false;
    }

    else{
        printf("I'm in cache!\t");
        printf("addr=%u, set=%d, tag=%u, offset=%u\n", addr, idx, tag, offset);
    
        // access the cache
        for(int i=0; i<ways; i++){
            cache_block* block = &cache->set[idx].way[i];
            // encounters invalid block
            if(block->valid == false){
                if(rd_done) break;
                else{
                    printf("cache invalid block - miss!\n");
                    rd_done = true;
                    block->valid = true;
                    block->tag = tag;
                    block->lru = 0;
                    fill_block(cache, idx, i, mem_addr);    //TODO: Can make it better?
                    read_data = block->value[offset];
                    uint32_t temp = mem_read_32(addr);
                    printf("cache_data = %u, mem_data=%u", read_data, temp);
                    
                    pipe.icache_stall = 50;
                    cache->mdata.cache_miss = true;
                    // cache_miss = true;
                }
            }

            // encounters valid block
            else{
                if(rd_done) block->lru++;                   // Already read-done --> just update lru
                else{
                    if(tag != block->tag){
                        printf("tag value - Expected = %u, Given = %u\t", tag, block->tag);                    
                        printf("tag not matching! not yet decided if hit or miss!\n");
                        block->lru++;                       // tag ain't matching --> just update
                        if(block->lru > max_lru){
                            evict_way = i;
                            max_lru = block->lru;
                        }
                    }
                    else{
                        printf("tag value - Expected = %u, Given = %u\t", tag, block->tag);
                        printf("cache hit!\n");
                        rd_done = true;
                        block->lru = 0;                     // tag matching --> block reused
                        // read-data
                        read_data = block->value[offset];
                        uint32_t temp = mem_read_32(addr);
                        printf("cache_data = %u, mem_data=%u\n", read_data, temp);
                    }
                }
            }

        }

        if(rd_done == false){
            printf("cache miss - eviction! - evicted block = %u\n", evict_way);
            cache_block* block = &cache->set[idx].way[evict_way];
            evict_block(cache, idx, evict_way);
            fill_block(cache, idx, evict_way, mem_addr);
            block->lru = 0;
            block->tag = tag;        
            read_data = block->value[offset];

            uint32_t temp = mem_read_32(addr);
            printf("cache_data = %u, mem_data=%u\n", read_data, temp);

            pipe.icache_stall = 50;
            cache->mdata.cache_miss = true;
            // cache_miss = true;
        }
    }
    // if(cache_miss)  stat_cycles+=50;

    return read_data;
}

// void cache_write(cache_unit* cache, uint32_t addr, uint32_t val){
//     // Meta-data values
//     uint32_t block_size = cache->mdata.block_size;
//     uint32_t ways = cache->mdata.ways;
//     uint32_t sets = cache->mdata.sets;

//     // cache-index calculation
//     uint32_t idx = (addr>>clog2(block_size)) & (sets-1);
//     uint32_t mem_addr = addr & (-1U<<clog2(block_size));
//     uint32_t tag = addr>>(sets+block_size);
//     uint32_t offset = addr & (block_size-1);

//     bool cache_miss = false;
//     bool wr_done = false;
//     uint32_t evict_way = 0, max_lru = 0;

//     for(int i=0; i<ways; i++){
//         cache_block* block = &cache->set[idx].way[i];
//         // Invalid way
//         if(block->valid == false){
//             cache_miss = true;
//             wr_done = true;
//             block->lru = 0;
//             block->valid = true;
//         }
//         // Valid way
//         else{

//         }
//     }
// }


uint32_t clog2(uint32_t x){
    uint32_t logx=-1;
    while(x){
        logx++;
        x = x>>1;
    }
    return logx;
}

// De-allocate cache
// void dealloc_cache(cache_unit* cache, uint32_t block_size, uint32_t ways, uint32_t sets){
//     for(int i=0; i<sets; i++){
//         for(int j=0; j<ways; j++){
//             free(cache->set[i].way[j].value);
//             cache->set[i].way[j].value = NULL;
//         }
//         free(cache->set[i].way);
//         cache->set[i].way = NULL;
//     }

//     free(cache->set);
//     cache->set = NULL;

//     free(cache);
//     cache = NULL;
// }