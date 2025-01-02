#include <spl.h>
#include <types.h>
#include <mips/tlb.h>
#include <vm_tlb.h>
#include <vmstats.h>

static int tlb_get_rr_victim(void)
{

    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}

void tlb_insert(vaddr_t vaddr, paddr_t paddr, uint8_t readonly)
{
    int spl;
    int victim;
    uint32_t ehi, elo;

    spl = splhigh();

    victim = tlb_get_rr_victim();

    tlb_read(&ehi, &elo, victim);
    if(elo & TLBLO_VALID)
    {
        vmstats_increment(TLB_FAULTS_WITH_REPLACE);
    }
    else
    {
        vmstats_increment(TLB_FAULTS_WITH_FREE);
    }

    ehi = vaddr;

    if(readonly == 1) // solo lettura
        elo = paddr | TLBLO_VALID;
    else
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;

    tlb_write(ehi, elo, victim);


    splx(spl);
}


void tlb_invalid(void)
{
    int i, spl;
    spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void tlb_invalid_one(paddr_t paddr)
{
    int i, spl;
    uint32_t ehi, elo;

    KASSERT((paddr & PAGE_FRAME) == paddr);

    spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
        tlb_read(&ehi, &elo, i);
        if((elo & PAGE_FRAME) == paddr)
        {
		    tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }
	}

	splx(spl);
}