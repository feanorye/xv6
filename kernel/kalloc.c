// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next; // equal to TreeNode without val
};

// freelist-> r1 -> r2 -> ...
struct {
  struct spinlock lock;
  struct run *freelist;
  int count;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++){
    initlock(&kmem[i].lock, "kmem");
    kmem[i].count = 0;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  printf("#cpu %d freerange\n",cpuid());
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();//turn off interrupt

  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  kmem[id].count++;
  release(&kmem[id].lock);

  pop_off();//restore interrupt
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();//turn off interrupt
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if (!r) {
    for (int i = 0; i < NCPU; i++) {
      if (i == id)
        continue;
      acquire(&kmem[i].lock);
      // fail1: when kmem[i].minCount != cur_count
      //        e.g) set para to [5, 1/10 * minCount]
      //             if minCount == 0, then cur_count == 0
      //             which would cause sbrk stop.
      if (kmem[i].count > 0) {
        // Add mem to kmem[id]
        kmem[id].freelist = kmem[i].freelist;

        int mv_cnt = kmem[i].count / 5 + 1;
        struct run *prev = 0;
        // Remove kmem[i] cur_count mem.
        for (int j = 0; j < mv_cnt; j++) {
          prev = kmem[i].freelist; // prev -> mv kmem[] tail
          kmem[i].freelist = prev->next;
        }
        // remove list connection
        prev->next = 0;
        // Fix kmem count
        kmem[i].count = kmem[i].count - mv_cnt;
        kmem[id].count = mv_cnt;
        // Need release kmem[i]
        release(&kmem[i].lock);
        // debug: disp_mem
        // disp_mem(id, i, cur_count);
        break;
      }
      release(&kmem[i].lock);
    }
    r = kmem[id].freelist;
  }
  if (r) {
    kmem[id].freelist = r->next;
    kmem[id].count--;
  }
  release(&kmem[id].lock);

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
disp_mem(int id, int bid, int mem){
  printf("CPU#%d <- CPU#%d: %d\n", id, bid, mem);
  for(int i = 0; i < NCPU; i++){
    printf("CPU#%d: mem: %d\n", i, kmem[i].count);
  }
}
