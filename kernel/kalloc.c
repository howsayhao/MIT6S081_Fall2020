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
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int link[PGROUNDUP(PHYSTOP)/PGSIZE];
} linkcount;

void
cow_linkclear(uint64 pa)
{ 
  acquire(&linkcount.lock);
  linkcount.link[pa/PGSIZE] = 0;
  release(&linkcount.lock);
}
void
cow_linkcline(uint64 pa)
{
  acquire(&linkcount.lock);
  linkcount.link[pa/PGSIZE] -= 1;
  release(&linkcount.lock);
}
void
cow_linkrise(uint64 pa)
{
  acquire(&linkcount.lock);
  linkcount.link[pa/PGSIZE] += 1;
  release(&linkcount.lock);  
}
int
cow_linkacquire(uint64 pa)
{
  return linkcount.link[pa/PGSIZE];
}

void
linkinit()
{
  initlock(&linkcount.lock, "linkcount");
  for(int i = 0; i < PGROUNDUP(PHYSTOP) / PGSIZE; i++){
    acquire(&linkcount.lock);
    linkcount.link[i] = 0;
    release(&linkcount.lock);
  }
}

void
kinit()
{
  linkinit();
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  for(int i = 0; i < PGROUNDUP(PHYSTOP) / PGSIZE; i++){
    acquire(&linkcount.lock);
    linkcount.link[i] = 0;
    release(&linkcount.lock);
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    struct run *r;
    if(((uint64)p % PGSIZE) != 0 || (char*)p < end || (uint64)p >= PHYSTOP)
      panic("kfree");
    memset(p, 1, PGSIZE);
    r = (struct run*)p;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
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
  acquire(&kmem.lock);
  cow_linkcline((uint64)pa);
  if(cow_linkacquire((uint64)pa) > 0) {
    release(&kmem.lock);
    return ;}
  //  else if(cow_linkacquire((uint64)pa) < 0)
    // panic("no!!!!!!!!!!!!");
  // else if(cow_linkacquire((uint64)pa) < 0) {
  //   panic("negative auote");
  // }
  release(&kmem.lock);

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 4, PGSIZE); // fill with junk
    acquire(&linkcount.lock);
    linkcount.link[(uint64)r/PGSIZE] = 1;
    release(&linkcount.lock);
  }
  return (void*)r;
}
