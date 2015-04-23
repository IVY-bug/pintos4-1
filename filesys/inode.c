#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/*************************************************/



/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
//    block_sector_t start;               /* First data sector. */
//    off_t length;                       /* File size in bytes. */
//    unsigned magic;                     /* Magic number. */
//    uint32_t unused[125];               /* Not used. */
      off_t length;
      unsigned magic;
      uint32_t direct_index;
      uint32_t indirect_index;
      uint32_t double_indirect_index;
      bool isDir;
      block_sector_t parent;
      uint32_t unused[107];
      block_sector_t pointer[14];
  };

/*
 * Indirect block, which are used to store pointers that point ot 
 * other data block
 */
struct indirect_block
{
    block_sector_t ptr[128];
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

static size_t
bytes_to_indirect_sectors (off_t size)
{
   if (size <= BLOCK_SECTOR_SIZE * 4)
   {
       return 0;
   }
   size -= BLOCK_SECTOR_SIZE * 4;
   return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE * 128);
}

static size_t
bytes_to_double_indirect_sector (off_t size)
{
   if (size <= BLOCK_SECTOR_SIZE * (4 + 128 * 9))
   {
       return 0;
   }
   return 1;
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  //  struct inode_disk data;             /* Inode content. */
    off_t length;                       //file size in bytes
    off_t read_length;
    size_t direct_index;
    size_t indirect_index;
    size_t double_indirect_index;
    bool isDir;
    block_sector_t parent;
    struct lock inode_lock;
    block_sector_t pointer[14];

   };
/**************methods *******************/
bool
inode_dealloc (struct inode *inode);
void
inode_dealloc_indirect_block (block_sector_t *ptr, size_t data_ptrs);
void
inode_dealloc_double_indirect_block (block_sector_t *ptr,
				     size_t indirect_ptrs,
				     size_t data_ptrs);
size_t
inode_expand_indirect_block (struct inode *inode, size_t new_data_sectors);
size_t
inode_expand_double_indirect_block (struct inode *inode, size_t new_data_sectors);

void
inode_lock (const struct inode *inode)
{
    lock_acquire (&((struct inode *)inode)->inode_lock);
}
void 
inode_unlock (const struct inode *inode)
{
    lock_release(&((struct inode *)inode)->inode_lock);
}
/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t length,  off_t pos) 
{
  ASSERT (inode != NULL);
 /* if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
 */
  if (pos < length)
  {
     uint32_t index;
     uint32_t indirect_block[128];
     if (pos < BLOCK_SECTOR_SIZE * 4)
     {
         return inode->pointer[pos/BLOCK_SECTOR_SIZE];
     }else if (pos < BLOCK_SECTOR_SIZE * (4 + 9 * 128))
     {
         pos -= BLOCK_SECTOR_SIZE * 4;
	 index = pos / (BLOCK_SECTOR_SIZE * 128) + 4;
	 block_read (fs_device, inode->pointer[index], &indirect_block);
         pos %= BLOCK_SECTOR_SIZE * 128;
         return indirect_block[pos / BLOCK_SECTOR_SIZE];
     }
     else
     {
	block_read (fs_device, inode->pointer[13], &indirect_block);
        pos -= BLOCK_SECTOR_SIZE * (4 + 9 * 128);
        index = pos / (BLOCK_SECTOR_SIZE * 128);
        block_read (fs_device, indirect_block[index], &indirect_block);
        pos %= BLOCK_SECTOR_SIZE * 128;
        return indirect_block[pos/BLOCK_SECTOR_SIZE];
     }
  }
    else
    {
       return -1;
    }

}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

off_t
inode_expand (struct inode *inode, off_t length)
{
    static char zeros[BLOCK_SECTOR_SIZE];
    size_t new_data_sectors = bytes_to_sectors(length)
			      - bytes_to_sectors(inode->length);
    if (new_data_sectors == 0)
    {
	return length;
    }
    
    while (inode->direct_index < 4)
    {
	free_map_allocate (1, &inode->pointer[inode->direct_index]);
	block_write(fs_device, inode->pointer[inode->direct_index], zeros);
        inode->direct_index++;
	new_data_sectors--;
	if (new_data_sectors == 0)
	{
	    return length;
	}
    }
    while (inode->direct_index < 13)
    {
	new_data_sectors = inode_expand_indirect_block (inode, new_data_sectors);
	if (new_data_sectors == 0)
	{
		return length;
	}
    }
    if (inode->direct_index == 13)
    {
	new_data_sectors = inode_expand_double_indirect_block (inode, new_data_sectors);
    }
    return length - new_data_sectors* BLOCK_SECTOR_SIZE;

}

/* alloc inode for the specific disk inode */
bool
inode_allocate (struct inode_disk *disk_inode)
{
     struct inode inode = {
         .length = 0,
	 .direct_index = 0,
	 .indirect_index = 0,
	 .double_indirect_index = 0,
      };
     inode_expand (&inode, disk_inode->length);
     disk_inode->direct_index = inode.direct_index;
     disk_inode->indirect_index = inode.indirect_index;
     disk_inode->double_indirect_index = inode.double_indirect_index;
     memcpy (&disk_inode->pointer, &inode.pointer, 14 * sizeof(block_sector_t));
     return true;
}



/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isDir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
     // size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->isDir = isDir;
      disk_inode->parent = ROOT_DIR_SECTOR;
      if (inode_allocate(disk_inode))
      {
          block_write (fs_device, sector, disk_inode);
	  success = true;
      }
      free (disk_inode);
    }
    /*
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }
  */
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
/* added code here */

  lock_init (&inode->inode_lock);
  struct inode_disk data;
  block_read (fs_device, inode->sector, &data);
  inode->length = data.length;
  inode->read_length = data.length;
  inode->direct_index = data.direct_index;
  inode->indirect_index = data.indirect_index;
  inode->double_indirect_index = data.double_indirect_index;
  inode->isDir = data.isDir;
  inode->parent = data.parent;
  memcpy (&inode->pointer, &data.pointer, 14 * sizeof(block_sector_t));
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
 //         free_map_release (inode->data.start,
 //                           bytes_to_sectors (inode->data.length)); 
           inode_dealloc(inode);
        }else
        {
            struct inode_disk disk_inode = {
    		.length = inode->length,
		.magic = INODE_MAGIC,
		.direct_index = inode->direct_index,
		.indirect_index = inode->indirect_index,
		.double_indirect_index = inode->double_indirect_index,
		.isDir = inode->isDir,
		.parent = inode->parent,
	     };
	    memcpy (&disk_inode.pointer, &inode->pointer, 14 * sizeof (block_sector_t));
            block_write (fs_device, inode->sector, &disk_inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
//  uint8_t *bounce = NULL;

 off_t length = inode->read_length;
 if (offset >= length)
  {
      return bytes_read;
  }
 
   while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode,length, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

/*      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
*/          /* Read full sector directly into caller's buffer. */
/*          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
*/          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
/*          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
*/      
     struct cache_block *c = cache_block_get (sector_idx, false);
     memcpy (buffer+bytes_read, (uint8_t *)&c->block+sector_ofs, chunk_size);
     c->accessed = true;
     c->open_cnt--;   
     /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
//  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
//  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;
  if (offset + size > inode_length (inode))
  {
	if (!inode->isDir)
	    {
		inode_lock(inode);
	    }
	inode->length = inode_expand (inode, offset+size);
	if (!inode->isDir)
	  {
		inode_unlock(inode);
	  }
   }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode,inode_length(inode),  offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

//      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
//        {
          /* Write full sector directly to disk. */
//          block_write (fs_device, sector_idx, buffer + bytes_written);
//        }
//      else 
//        {
          /* We need a bounce buffer. */
//          if (bounce == NULL) 
//            {
//              bounce = malloc (BLOCK_SECTOR_SIZE);
//              if (bounce == NULL)
//                break;
//            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
//          if (sector_ofs > 0 || chunk_size < sector_left) 
//            block_read (fs_device, sector_idx, bounce);
//          else
//            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          struct cache_block *c = cache_block_get (sector_idx, true);
          memcpy ((uint8_t *)&c->block+sector_ofs,buffer+bytes_written,  chunk_size);
          c->accessed = true;
  	  c->dirty = true;
 	  c->open_cnt--;
//          block_write (fs_device, sector_idx, bounce);
//        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
//  free (bounce);
  inode->read_length = inode_length(inode);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}
/* dealloc indirect block */
void
inode_dealloc_indirect_block (block_sector_t *ptr, size_t data_ptrs)
{
     unsigned int i;
     struct indirect_block block;
     block_read(fs_device, *ptr, &block);
     for (i = 0; i < data_ptrs; i++)
     {
	free_map_release (block.ptr[i],1);
     }
     free_map_release (*ptr, 1);
}
void
inode_dealloc_double_indirect_block (block_sector_t *ptr, size_t indirect_ptrs,size_t data_ptrs)
{
     unsigned int i;
     struct indirect_block block;
     block_read(fs_device, *ptr, &block);
     for (i = 0; i < indirect_ptrs; i++)
     {
         size_t data_per_block = data_ptrs < 128 ? data_ptrs : 128;
         inode_dealloc_indirect_block (&block.ptr[i], data_per_block);
	 data_ptrs -= data_per_block;
     }
     free_map_release (*ptr,1);
}


/*dealoc inode */
bool 
inode_dealloc (struct inode *inode)
{
    size_t data_sectors = bytes_to_sectors(inode->length);
    size_t indirect_sectors = bytes_to_indirect_sectors(inode->length);
    size_t double_indirect_sector = bytes_to_double_indirect_sector (inode->length);
    unsigned int index = 0;
    while (data_sectors && index < 4)
    {
       free_map_release (inode->pointer[index],1);
       data_sectors--;
       index++;
    }
    while (indirect_sectors && index < 13)
    {
        size_t data_ptrs = data_sectors < 128 ? data_sectors: 128;
	inode_dealloc_indirect_block (&inode->pointer[index], data_ptrs);
        data_sectors -= data_ptrs;
 	indirect_sectors--;
        index++;
    }
    if (double_indirect_sector)
    {
       inode_dealloc_double_indirect_block(&inode->pointer[index], indirect_sectors,data_sectors);
    }
}
/* expand indirect block */
size_t
inode_expand_indirect_block (struct inode *inode, size_t new_data_sectors)
{
     static char zeros[BLOCK_SECTOR_SIZE];
     struct indirect_block block;
     if (inode->indirect_index == 0)
     {
	free_map_allocate (1, &inode->pointer[inode->direct_index]);
     }
     else
     {
	block_read (fs_device, inode->pointer[inode->direct_index], &block);
     }
     while (inode->indirect_index < 128)
     {
	free_map_allocate (1, &block.ptr[inode->indirect_index]);
	block_write (fs_device, block.ptr[inode->indirect_index], zeros);
	inode->indirect_index++;
	new_data_sectors--;
	if (new_data_sectors == 0)
	{
	    break;
	}
      }
      block_write (fs_device, inode->pointer[inode->direct_index], &block);
      if (inode->indirect_index == 128)
	{
	     inode->indirect_index = 0;
	     inode->direct_index ++;
	}
     return new_data_sectors;
}


size_t inode_expand_double_indirect_block_lvl_two (struct inode *inode,
					   size_t new_data_sectors,
					   struct indirect_block* outer_block)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  struct indirect_block inner_block;
  if (inode->double_indirect_index == 0)
    {
      free_map_allocate(1, &outer_block->ptr[inode->indirect_index]);
    }
  else
    {
      block_read(fs_device, outer_block->ptr[inode->indirect_index],
		 &inner_block);
    }
  while (inode->double_indirect_index < 128)
    {
      free_map_allocate(1, &inner_block.ptr[inode->double_indirect_index]);
      block_write(fs_device, inner_block.ptr[inode->double_indirect_index],
		  zeros);
      inode->double_indirect_index++;
      new_data_sectors--;
      if (new_data_sectors == 0)
	{
	  break;
	}
    }
  block_write(fs_device, outer_block->ptr[inode->indirect_index], &inner_block);
  if (inode->double_indirect_index == 128)
    {
      inode->double_indirect_index = 0;
      inode->indirect_index++;
    }
  return new_data_sectors;
}



/* expand double indirect block */
size_t
inode_expand_double_indirect_block (struct inode *inode, size_t new_data_sectors)
{
      struct indirect_block block;
      if (inode->double_indirect_index == 0 && inode->indirect_index == 0)
	{
		free_map_allocate (1, &inode->pointer[inode->direct_index]);
	}
       else
	{
		block_read (fs_device, inode->pointer[inode->direct_index],&block);
	}
	while (inode->indirect_index < 128)
	{
 		new_data_sectors = inode_expand_double_indirect_block_lvl_two(inode,new_data_sectors, &block);
		if (new_data_sectors == 0)
		{
		   break;
		}
	}
	block_write (fs_device, inode->pointer[inode->direct_index], &block);
	return new_data_sectors;
}
