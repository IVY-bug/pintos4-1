#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"

void
thread_func_read_ahead (void *aux);
struct cache_block*
read_block_from_disk (block_sector_t sector, bool dirty);
void
thread_func_flush_back (void *aux);
struct cache_block* 
cache_block_evict (block_sector_t sector, bool dirty);



/*
 *  Initialize all the cache related struct
 */
void
cache_init (void)
{
    list_init (&cache);
    lock_init (&cache_lock);
    cache_size = 0;         
    thread_create ("cache_flush_back", 0, thread_func_flush_back, NULL);
}

/*
 *  To check whether or not given sector is inside
 *  current cache. if yes, return this cache block back.
 */
struct cache_block* block_in_cache (block_sector_t sector)
{
    struct cache_block *block;
    struct list_elem *e;
    for (e = list_begin (&cache); e != list_end (&cache); e = list_next(e))
    {
	block = list_entry (e, struct cache_block, elem);
  	if (block ->sector == sector)
	   {
		return block;
	   }
     }
     return NULL;
}	
    
/*
 * When a request is made to read a block, check to see if it is
 * in the cache, and if so, use the cached data without going to disk
 * Otherwise, fetch the block from disk into the cache, evivcting an
 * older entry if neccessary.  The cache size limited no greater than
 * 64 sectors in size.\
 * 1. If there is a hit of the required block in the cache,return back
 * 2. If there is a miss, and there is cache_block available, read from disk
 * 3. If there is a miss, and the cache is full, evict one, replace it.
 */

struct cache_block* cache_block_get (block_sector_t sector, bool dirty)
{
    lock_acquire (&cache_lock);
    struct cache_block *c = block_in_cache (sector);
    if (c)
    {
	c->open_cnt ++;
	c->dirty |= dirty;
	c->accessed = true;
	lock_release (&cache_lock);
	return c;
     }
     if (cache_size < CACHE_AMOUNT)
     {
         c = read_block_from_disk (sector, dirty);
         lock_release (&cache_lock);
	 return c;
     }else{
         c = cache_block_evict (sector, dirty);
     }
     lock_release (&cache_lock);
     return c;
}

/*
 * Fetch block from disk
 * Haven't do synchronization here, not sure whether ok????????? 
 */
struct cache_block* read_block_from_disk (block_sector_t sector, bool dirty)
{
    
//    lock_acquire (&cache_lock);
    cache_size ++;
    struct cache_block *c = malloc (sizeof (struct cache_block));
    if (!c)
    {
        PANIC ("Not enought memory for buffer cache");
    }
    c->open_cnt = 1;
    list_push_back (&cache, &c->elem);
    c->sector = sector;
    block_read (fs_device, c->sector, &c->block);
    c->dirty = dirty;
    c->accessed = true;
//    lock_release (&cache_lock);
    return c;
}

/*
 * Instead of reading from disk directly, due to the limit of cache size,
 * it is neccessay to evict one cache block using clock algorithm, and then
 * fetch the block.
 */
struct cache_block* cache_block_evict (block_sector_t sector, bool dirty)
{

    struct cache_block *c;
/*    if (cache_size < MAX_CACHE_SIZE)
    {
        cache_size++;
        c = malloc (sizeof (struct cache_block));
        if (!c)
        {
	    return NULL;
	}
	c->open_cnt = 0;
	list_push_back (&cache, &c->elem);
    }
    else
    {
  */    bool flag = true;
      while (flag)
      {
        struct list_elem *e;
        for (e = list_begin (&cache); e != list_end (&cache); e = list_next(e))
        {
		c = list_entry (e, struct cache_block, elem);
                if (c->open_cnt > 0)
                {
			continue;
		}
		if (c->accessed)
		{
			c->accessed = false;
		}
		else
		{
			if (c->dirty)
			{
			   block_write (fs_device, c->sector, &c->block);
			}
		        flag = false;
			break;
  		}
 	}
      }
   //  }
    //pay attention to here!!!!!!!!
    c->open_cnt++;
    c->sector = sector;
    block_read (fs_device, c->sector, &c->block);
    c->dirty = dirty;
    c->accessed = true;
    return c;
}
    

/*
 *  flush dirty cache block back to disk.
 */

/*
void
cache_flush_to_disk (bool halt)
{
   lock_acquire (&cache_lock);
   struct list_elem *e;
   for ( e = list_begin (&cache); e != list_end(&cache); e = list_next (e))
   {
       struct cache_block* c = list_entry (e, struct cache_block, elem);
       if (c->dirty)
	{
	   block_write (fs_device, c->sector, &c->block);
	   c->dirty = false;
        }
	if (halt)
	{
	  list_remove (&c->elem);
	  free (c);
	}
    }
   lock_release (&cache_lock);
}
*/
void cache_flush_to_disk (bool halt)
{
  lock_acquire(&cache_lock);
  struct list_elem *next, *e = list_begin(&cache);
  while (e != list_end(&cache))
    {
      next = list_next(e);
      struct cache_block *c = list_entry(e, struct cache_block, elem);
      if (c->dirty)
	{
	  block_write (fs_device, c->sector, &c->block);
	  c->dirty = false;
	}
      if (halt)
	{
	  list_remove(&c->elem);
	  free(c);
	}
      e = next;
    }
  lock_release(&cache_lock);
}

/*
 * Periodically flush dirty cache back to disk
 */
void
thread_func_flush_back (void *aux UNUSED)
{
   while (true)
   {
	timer_sleep (FLUSH_BACK_INTERVAL);
        cache_flush_to_disk (false);
   }
}


/*
 * Read-ahead implementation.  
 */
void
read_ahead (block_sector_t sector)
{
   block_sector_t *arg = malloc (sizeof (block_sector_t));
   if (arg)
   {
	*arg = sector + 1;
        thread_create ("cache_read_ahead", 0, thread_func_read_ahead, arg);
   }
}

void
thread_func_read_ahead (void *aux)
{
    block_sector_t sector = * (block_sector_t *)aux;
    lock_acquire (&cache_lock);
    struct cache_block *c = block_in_cache (sector);
    if (!c)
    {
        cache_block_get (sector, false);
    }
    lock_release (&cache_lock);
    free (aux);
}





