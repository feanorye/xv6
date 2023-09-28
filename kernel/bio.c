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

struct bucket{
  struct spinlock latch_;
  struct buf head_;
  int free_num_;
};
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i = 0; i < NBUCKET; i++){
    initlock(&(bcache.bucket[i].latch_), "bcache.bucket");
    bcache.bucket[i].head_.next = &(bcache.bucket[i].head_);
    bcache.bucket[i].head_.prev = &(bcache.bucket[i].head_);
    bcache.bucket[i].free_num_ = 0;
  }
  // Create linked list of buffers
  struct buf *head = &(bcache.bucket[0].head_);
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->prev = head;
    b->next = head->next;
    initsleeplock(&b->lock, "buffer");
    head->next->prev = b;
    head->next = b;
  }
  bcache.bucket[0].free_num_ = NBUF;
}

void
bprint(int idx, char *prefix){
  struct buf *head = &bcache.bucket[idx].head_;
  printf("%s bucket: %d, freenum: %d: ", prefix, idx,
         bcache.bucket[idx].free_num_);
  int i = 0;
  for (struct buf *b = head->next; b != head; b = b->next, i++) {
    printf("(%d:%d),", i, b->refcnt);
  }
  printf("\n");
}

/**
 * @brief steal evictble buf from other bucket.
 * @param req_idx idx of request bucket idx. Caller should hold it's latch_
 */
static struct buf *
stealbuf(int req_idx) {
  // steal from other bucket. NOTE: deadlock may happen
  // when bucket1 scan bucket3 for free, buket3 scan bucket1 for free buf.
  release(&bcache.bucket[req_idx].latch_);
  struct buf *eb = 0;

  for (int i = 1, ei; i < NBUCKET; i++) {
    ei = (req_idx + i) % NBUCKET;
    acquire(&bcache.bucket[ei].latch_);
    if (bcache.bucket[ei].free_num_ == 0) {
      release(&bcache.bucket[ei].latch_);
      continue;
    }
    if(bcache.bucket[ei].free_num_ < 0){
      panic("ERROR: bucket free_num < 0\n");
    }
    for (struct buf *b = bcache.bucket[ei].head_.next;
         b != &(bcache.bucket[ei].head_); b = b->next) {
      if (b->refcnt == 0 && (eb == 0 || b->rtime < eb->rtime)) {
        eb = b;
      }
    }
    if(eb == 0){
      bprint(ei, "panic");
      panic("steal 0 buf\n");
    }
    bprint(ei, "#del");
    eb->next->prev = eb->prev;
    eb->prev->next = eb->next;
    bcache.bucket[ei].free_num_--;
    release(&bcache.bucket[ei].latch_);
    break;
  }

  acquire(&bcache.bucket[req_idx].latch_);
  if (eb != 0) {
    eb->next = bcache.bucket[req_idx].head_.next;
    eb->prev = &bcache.bucket[req_idx].head_;
    bcache.bucket[req_idx].head_.next->prev = eb;
    bcache.bucket[req_idx].head_.next = eb;
    bcache.bucket[req_idx].free_num_++;
  }
  bprint(req_idx, "#add");
  return eb;
}

// 1. Look through buffer cache for block on device dev.
// 2. If not found, allocate a lease used buffer from evictable buffers.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *eb = 0;
  int bucketi = blockno % NBUCKET;
  acquire(&bcache.bucket[bucketi].latch_);

  // Is the block already cached?
  for (struct buf *b = bcache.bucket[bucketi].head_.next;
       b != &(bcache.bucket[bucketi].head_); b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      // refcnt could ensure buf not reused
      release(&bcache.bucket[bucketi].latch_);
      acquiresleep(&b->lock);
      return b;
    }
    if (b->refcnt == 0 && (eb == 0 || b->rtime < eb->rtime)) {
      eb = b;
    }
  }

  if (eb != 0 || (eb = stealbuf(bucketi)) != 0) {
    eb->dev = dev;
    eb->blockno = blockno;
    eb->valid = 0;
    eb->refcnt = 1;
    bcache.bucket[bucketi].free_num_--;
    release(&bcache.bucket[bucketi].latch_);
    acquiresleep(&eb->lock);
    return eb;
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
  acquire(&bcache.bucket[bi].latch_);
  b->refcnt--;
  // Only care rtime of evictable buffer
  if (b->refcnt == 0) {
    bcache.bucket[bi].free_num_++;
    b->rtime = ticks;
    bprint(bi, "#release ");
  }
  release(&bcache.bucket[bi].latch_);
}

// todo: log为什么要pin? 什么情况下unpin?
void
bpin(struct buf *b) {
  int bi = b->blockno % NBUCKET;
  acquire(&bcache.bucket[bi].latch_);
  b->refcnt++;
  release(&bcache.bucket[bi].latch_);
}

void
bunpin(struct buf *b) {
  int bi = b->blockno % NBUCKET;
  acquire(&bcache.bucket[bi].latch_);
  b->refcnt--;
  release(&bcache.bucket[bi].latch_);
}


