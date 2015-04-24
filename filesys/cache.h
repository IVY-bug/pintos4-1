#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include <list.h>

#define CACHE_AMOUNT 64
#define FLUSH_BACK_INTERVAL 5*TIMER_FREQ
#define MAX_CACHE_SIZE 64
struct cache_block
{
    block_sector_t sector;
    bool dirty;
    bool accessed;
    int open_cnt;
    struct list_elem elem;
    uint8_t block[BLOCK_SECTOR_SIZE];
};

struct lock cache_lock;
struct list cache;
uint32_t cache_size;


void cache_init (void);

struct cache_block* cache_block_get(block_sector_t sector, bool dirty);
void cache_flush_to_disk (bool halt);
void read_ahead (block_sector_t sector);




#endif
