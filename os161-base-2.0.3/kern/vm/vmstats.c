#include <vmstats.h>
#include <spinlock.h>
#include <lib.h>


static struct spinlock vmstats_lock = SPINLOCK_INITIALIZER;
static struct statistics* vmstats = NULL;
static int vmstats_active = 0;

static int vmstats_isactive(void)
{
    int active;
	spinlock_acquire(&vmstats_lock);
	active = vmstats_active;
	spinlock_release(&vmstats_lock);
	return active;
}

void vmstats_init(void)
{
    vmstats = kmalloc(sizeof(struct statistics));
    if(vmstats == NULL)
    {
        panic("Failed stats initialization\n");
    }

    vmstats->tlb_faults = 0;
    vmstats->tlb_faults_with_free = 0;
    vmstats->tlb_faults_with_replace = 0;
    vmstats->tlb_invalidations = 0;
    vmstats->tlb_reloads = 0;
    vmstats->page_faults_zeroed = 0;
    vmstats->page_faults_disk = 0;
    vmstats->page_faults_elf = 0;
    vmstats->page_faults_swap = 0;
    vmstats->swapfile_writes = 0;

    spinlock_acquire(&vmstats_lock);
    vmstats_active = 1;
    spinlock_release(&vmstats_lock);

}

void vmstats_shutdown(void)
{
    //stampo le statistiche 

    kprintf("tlb_faults = %d\n", vmstats->tlb_faults);
    kprintf("tlb_faults_with_free = %d\n", vmstats->tlb_faults_with_free);
    kprintf("tlb_faults with replace = %d\n", vmstats->tlb_faults_with_replace);
    kprintf("tlb_invalidation = %d\n", vmstats->tlb_invalidations);
    kprintf("tlb_reloads = %d\n", vmstats->tlb_reloads);
    kprintf("page fault zeroed = %d\n", vmstats->page_faults_zeroed);
    kprintf("page fault disk = %d\n", vmstats->page_faults_disk);
    kprintf("page fault elf = %d\n", vmstats->page_faults_elf);
    kprintf("page fault swap = %d\n", vmstats->page_faults_swap);
    kprintf("swapfile writes = %d\n", vmstats->swapfile_writes);

    if(vmstats->tlb_faults != vmstats->tlb_faults_with_free + vmstats->tlb_faults_with_replace)
    {
        kprintf("WARNING: Il conteggio di tlb faults non è coerente con tlb faults with free e tlb fault with replacement");
    }

    if(vmstats->tlb_faults != vmstats->tlb_reloads + vmstats->page_faults_zeroed + vmstats->page_faults_disk)
    {
        kprintf("WARNING: Il conteggio di tlb faults non è coerente con tlb reloads, page fault zeroed e page fault disk");

    }

    /* Deactivate vmstats */
	spinlock_acquire(&vmstats_lock);
	vmstats_active = 0;
	spinlock_release(&vmstats_lock);
	
	kfree(vmstats);
}

void vmstats_increment(int code)
{
    if(!vmstats_isactive())
    {
        return;
    }


    spinlock_acquire(&vmstats_lock);
    
    switch (code)
    {
    case TLB_FAULTS:
        vmstats->tlb_faults += 1;
        break;
    case TLB_FAULTS_WITH_FREE:
        vmstats->tlb_faults_with_free += 1;
        break;
    case TLB_FAULTS_WITH_REPLACE:
        vmstats->tlb_faults_with_replace += 1;
        break;
    case TLB_INVALIDATIONS:
        vmstats->tlb_invalidations += 1;
        break;
    case TLB_RELOADS:
        vmstats->tlb_reloads += 1;
        break;
    case PAGE_FAULTS_ZEROED:
        vmstats->page_faults_zeroed += 1;
        break;
    case PAGE_FAULTS_DISK:
        vmstats->page_faults_disk += 1;
        break;
    case PAGE_FAULTS_ELF:
        vmstats->page_faults_elf += 1;
        break;
    case PAGE_FAULTS_SWAP:
        vmstats->page_faults_swap += 1;
        break;
    case SWAPFILE_WRITES:
        vmstats->swapfile_writes += 1;
        break;
    default:
        panic("Statistic code not recognized\n");
        break;
    }

    spinlock_release(&vmstats_lock);

}