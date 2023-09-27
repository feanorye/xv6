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
// * When done with the buffer, call brelease.
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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf freelist;
  struct buf bucket[NBUCKET];
  struct spinlock bk_lock[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i = 0; i < NBUCKET; i++){
    initlock(&(bcache.bk_lock[i]), "bcache.bucket");
    bcache.bucket[i].next = &bcache.bucket[i];
    bcache.bucket[i].prev = &bcache.bucket[i];
  }

  bcache.freelist.next = &bcache.freelist;
  bcache.freelist.prev = &bcache.freelist;
  // Create linked list of buffers
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->prev = &(bcache.freelist);
    b->next = bcache.freelist.next;
    initsleeplock(&b->lock, "buffer");
    b->next->prev = b;
    bcache.freelist.next = b;
  }
}

// 1. Look through buffer cache for block on device dev.
// 2. If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *eblock;
  int bucketi = blockno % NBUCKET;
  acquire(&bcache.bk_lock[bucketi]);

  // Is the block already cached?
  for(eblock = bcache.bucket[bucketi].next; eblock != &bcache.bucket[bucketi]; eblock = eblock->next){
    if(eblock->dev == dev && eblock->blockno == blockno){
      eblock->refcnt++;
      // refcnt could ensure buf not reused
      release(&bcache.bk_lock[bucketi]);
      acquiresleep(&eblock->lock);
      return eblock;
    }
  }
  acquire(&bcache.lock);
  eblock = bcache.freelist.next;
  if (eblock != &bcache.freelist) {
    eblock->next->prev = eblock->prev;
    eblock->prev->next = eblock->next;
  }
  release(&bcache.lock);

  if (eblock != &bcache.freelist) {
    eblock->next = bcache.bucket[bucketi].next;
    eblock->prev = &(bcache.bucket[bucketi]);

    eblock->next->prev = eblock;
    bcache.bucket[bucketi].next = eblock;
    eblock->dev = dev;
    eblock->blockno = blockno;
    eblock->valid = 0;
    eblock->refcnt = 1;
    release(&bcache.bk_lock[bucketi]);
    acquiresleep(&eblock->lock);
    return eblock;
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
  if (!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);

  int bi = b->blockno % NBUCKET;
  acquire(&bcache.bk_lock[bi]);
  b->refcnt--;
  if (b->refcnt > 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[bi].next;
    b->prev = &bcache.bucket[bi];
    bcache.bucket[bi].next->prev = b;
    bcache.bucket[bi].next = b;
  } else {
    b->next->prev = b->prev;
    b->prev->next = b->next;
    acquire(&bcache.lock);
    b->next = bcache.freelist.next;
    b->prev = &(bcache.freelist);
    bcache.freelist.next = b;
    b->next->prev = b;
    release(&bcache.lock);
  }

  release(&bcache.bk_lock[bi]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


