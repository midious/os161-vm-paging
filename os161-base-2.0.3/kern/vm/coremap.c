#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <addrspace.h>
#include <coremap.h>
//#include <swapfile.h>

static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
static int coremapActive = 0;

/*
 * Checks if the coremap is active
 */
static int isCoremapActive()
{
	int active;
	spinlock_acquire(&coremap_lock);
	active = coremapActive;
	spinlock_release(&coremap_lock);
	return active;
}


//VERSIONE DA MODIFICARE
/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

/*
 * Free the range of memory pages that have been
 * previously allocated to the kernel by alloc_kpages()
 * Also used in kfree()
 */
void free_kpages(vaddr_t addr)
{
	if (isCoremapActive())
	{
		paddr_t paddr = addr - MIPS_KSEG0;
		long first = paddr / PAGE_SIZE;
		KASSERT(nRamFrames > first);
		freeppages(paddr, coremap[first].allocSize);
	}
}

static paddr_t
getppages(unsigned long npages)
{
  paddr_t addr;

  /* try freed pages first */
  addr = getfreeppages(npages);
  if (addr == 0) {
    /* call stealmem */
    spinlock_acquire(&stealmem_lock);
    addr = ram_stealmem(npages);
    spinlock_release(&stealmem_lock);
  }
  if (addr!=0 && isTableActive()) {
    spinlock_acquire(&freemem_lock);
    allocSize[addr/PAGE_SIZE] = npages;
    spinlock_release(&freemem_lock);
  } 

  return addr;
}

static paddr_t 
getfreeppages(unsigned long npages) {
  paddr_t addr;	
  long i, first, found, np = (long)npages;

  if (!isCoremapActive()) return 0; 
  spinlock_acquire(&freemem_lock);
  for (i=0,first=found=-1; i<nRamFrames; i++) {
    if (freeRamFrames[i]) {
      if (i==0 || !freeRamFrames[i-1]) 
        first = i; /* set first free in an interval */   
      if (i-first+1 >= np) {
        found = first;
        break;
      }
    }
  }
}

/*
 *	Free a desired number of pages starting from addr.
 *	These free pages are now managed by coremap and not the
 *	lower level ram module.
 */
static int freeppages(paddr_t addr, unsigned long npages)
{
	long i, first, np = (long)npages;

	if (!isCoremapActive())
		return 0;
	first = addr / PAGE_SIZE;
	KASSERT(nRamFrames > first);

	spinlock_acquire(&coremap_lock);
	for (i = first; i < first + np; i++)
	{
		coremap[i].entry_type = COREMAP_FREED;
		coremap[i].vaddr = 0;
		coremap[i].as = NULL;
	}
	coremap[first].allocSize = 0;
	spinlock_release(&coremap_lock);

	return 1;
}