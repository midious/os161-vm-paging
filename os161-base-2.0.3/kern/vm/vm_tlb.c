#include <spl.h>
#include <mips/tlb.h>
#include <vm_tlb.h>

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

    ehi = vaddr;

    if(readonly == 1) // solo lettura
        elo = paddr | TLBLO_VALID;
    else
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;

    tlb_write(ehi, elo, victim);


    splx(spl);
}