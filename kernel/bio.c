// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define NBUCKETS 13
struct {
  struct spinlock global_lock;
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.global_lock, "bcache");
  for (int i = 0;i < NBUCKETS; i++) {
    initlock(&bcache.lock[i], "bcache_bucket");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }

  // Create linked list of buffers
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int blockno_key = blockno % NBUCKETS;
  acquire(&bcache.lock[blockno_key]);

  // Is the block already cached?
  for(b = bcache.head[blockno_key].next; b != &bcache.head[blockno_key]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[blockno_key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for(b = bcache.head[blockno_key].prev; b != &bcache.head[blockno_key]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[blockno_key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.lock[blockno_key]);



  acquire(&bcache.global_lock);
  acquire(&bcache.lock[blockno_key]);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (int i = 0; i < NBUCKETS; i++) {
    if (i != blockno_key) {
      acquire(&bcache.lock[i]);
      for (b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev) {
        if (b->refcnt == 0) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;

          
          b->next->prev = b->prev;
          b->prev->next = b->next;
          
          b->next = bcache.head[blockno_key].next;
          b->prev = &bcache.head[blockno_key];
          bcache.head[blockno_key].next->prev = b;
          bcache.head[blockno_key].next = b;
          release(&bcache.lock[i]);
          release(&bcache.lock[blockno_key]);
          release(&bcache.global_lock);

          acquiresleep(&b->lock);
          return b;

        }
      }
      release(&bcache.lock[i]);
    }
  }
  release(&bcache.lock[blockno_key]);
  release(&bcache.global_lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int b_key = b->blockno % NBUCKETS;
  acquire(&bcache.lock[b_key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[b_key].next;
    b->prev = &bcache.head[b_key];
    bcache.head[b_key].next->prev = b;
    bcache.head[b_key].next = b;
  }
  
  release(&bcache.lock[b_key]);
}

void
bpin(struct buf *b) {
  int b_key = b->blockno % NBUCKETS;
  acquire(&bcache.lock[b_key]);
  b->refcnt++;
  release(&bcache.lock[b_key]);
}

void
bunpin(struct buf *b) {
  int b_key = b->blockno % NBUCKETS;
  acquire(&bcache.lock[b_key]);
  b->refcnt--;
  release(&bcache.lock[b_key]);
}


