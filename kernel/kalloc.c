// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define INDEX(pa) ((pa - KERNBASE)>>12)
#define INDEXSIZE 32768//由KERNBASE到PHYSTOP共32768个页面
 
struct {
  struct spinlock lock;
  int count[INDEXSIZE];
} memref;

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

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&memref.lock,"memref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    
    acquire(&memref.lock);
    memref.count[INDEX((uint64)p)]=1;
    release(&memref.lock);
    kfree(p);
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
  
 
  acquire(&memref.lock);
  if (memref.count[INDEX((uint64)pa)]>1)
  {
    --memref.count[INDEX((uint64)pa)];
    release(&memref.lock);
    return;
  }
  release(&memref.lock);
  
 
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
 
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
 
  r = (struct run*)pa;
 
  acquire(&memref.lock);
  if (memref.count[INDEX((uint64)pa)]==0)
  {
    release(&memref.lock);
    panic("kfree");
  }
  else if (memref.count[INDEX((uint64)pa)]==1)
  {
    --memref.count[INDEX((uint64)pa)];
    release(&memref.lock);
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  else
  {
    release(&memref.lock);
    panic("kfree");
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void addref(uint64 pa)
{
  acquire(&memref.lock);
  memref.count[INDEX(pa)]++;
  release(&memref.lock);
}
 
 
void *
kalloc(void)
{
  struct run *r;
 
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    kmem.freelist = r->next;
    addref((uint64)r);
  }
  release(&kmem.lock);
 
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
