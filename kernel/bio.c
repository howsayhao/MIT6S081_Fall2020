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

#define NUMBUCKET (13)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

} bcache;

struct 
{
  struct spinlock lock;
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bucket[NUMBUCKET];


void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for(int i = 0; i < NUMBUCKET; i++)
  {
    initlock(&bucket[i].lock, "bcache");
    bucket[i].head.prev = &bucket[i].head;
    bucket[i].head.next = &bucket[i].head;
    for(b = bcache.buf+i; b < bcache.buf+NBUF; b+=NUMBUCKET){
        b->next = bucket[i].head.next;
        b->prev = &bucket[i].head;
        initsleeplock(&b->lock, "buffer");
        bucket[i].head.next->prev = b;
        bucket[i].head.next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);

  // Is the block already cached?
  int bucketno = blockno % NUMBUCKET;
  acquire(&bucket[bucketno].lock);
  for(b = bucket[bucketno].head.next; b != &bucket[bucketno].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      // release(&bcache.lock);
      release(&bucket[bucketno].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf *lrubuf = 0;
  for(b = bucket[bucketno].head.prev; b != &bucket[bucketno].head; b = b->prev){
    if(b->refcnt==0&&(lrubuf==0||lrubuf->timestamp>b->timestamp))lrubuf = b;
  }
  if(lrubuf != 0){
    lrubuf->dev = dev;
    lrubuf->blockno = blockno;
    lrubuf->valid = 0;
    lrubuf->refcnt = 1;
    release(&bucket[bucketno].lock);
    // release(&bcache.lock);
    acquiresleep(&lrubuf->lock);
    return lrubuf;
  }

  //no free buffer, steal from other buckets
  // int stealfrom = bucketno;
  release(&bucket[bucketno].lock);
  acquire(&bcache.lock);
  acquire(&bucket[bucketno].lock);
  for(b = bucket[bucketno].head.next; b != &bucket[bucketno].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      // release(&bcache.lock);
      release(&bucket[bucketno].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  lrubuf = 0;
  for(b = bucket[bucketno].head.prev; b != &bucket[bucketno].head; b = b->prev){
    if(b->refcnt==0&&lrubuf==0)lrubuf = b;
    else if(b->refcnt==0&&(lrubuf!=0&&lrubuf->timestamp>b->timestamp))lrubuf = b;
  }
  if(lrubuf != 0){
    lrubuf->dev = dev;
    lrubuf->blockno = blockno;
    lrubuf->valid = 0;
    lrubuf->refcnt = 1;
    release(&bucket[bucketno].lock);
    release(&bcache.lock);
    acquiresleep(&lrubuf->lock);
    return lrubuf;
  }


  for(int i = 0; i < NUMBUCKET; i++)
  {
    if(i == bucketno)
      continue;
    acquire(&bucket[i].lock);
    for(b = bucket[i].head.prev; b != &bucket[i].head; b = b->prev)
    {
      if(lrubuf == 0 && b->refcnt == 0){
        lrubuf = b;
        // stealfrom = i;
        continue;
      }
      if(b->refcnt == 0 && lrubuf->timestamp > b->timestamp){
        lrubuf = b;
        // stealfrom = i;
      }
    }
    release(&bucket[i].lock);
  }
  if(lrubuf != 0){
    lrubuf->prev->next = lrubuf->next;
    lrubuf->next->prev = lrubuf->prev;
    // release(&bucket[stealfrom].lock);

    // acquire(&bucket[bucketno].lock);
    lrubuf->next = bucket[bucketno].head.next;
    lrubuf->prev = &bucket[bucketno].head;
    bucket[bucketno].head.next->prev = lrubuf;
    bucket[bucketno].head.next = lrubuf;

    lrubuf->dev = dev;
    lrubuf->blockno = blockno;
    lrubuf->valid = 0;
    lrubuf->refcnt = 1;
    release(&bcache.lock);
    release(&bucket[bucketno].lock);
    // release(&bucket[stealfrom].lock);
    acquiresleep(&lrubuf->lock);
    return lrubuf;
  }
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

  int bucketno = b->blockno % NUMBUCKET;
  acquire(&bucket[bucketno].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks;
  }
  
  release(&bucket[bucketno].lock);
}

void
bpin(struct buf *b) {
  int bucketno = b->blockno % NUMBUCKET;
  acquire(&bucket[bucketno].lock);
  b->refcnt++;
  release(&bucket[bucketno].lock);
}

void
bunpin(struct buf *b) {
  int bucketno = b->blockno % NUMBUCKET;
  acquire(&bucket[bucketno].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks;
  }
  release(&bucket[bucketno].lock);
}


