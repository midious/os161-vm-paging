#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

#include <pt.h>
#include <segments.h>

struct entry* get_pt_entry(vaddr_t vaddr, struct addrspace *as) {
    // Segmento code
    vaddr_t vbase1 = as->page_table->code->v_base;
    vaddr_t vtop1 = vbase1 + as->page_table->code->npages * PAGE_SIZE;

    if (vaddr >= vbase1 && vaddr < vtop1) {
        int index = (vaddr - vbase1) / PAGE_SIZE;
        return &as->page_table->code->entries[index];
    }

    // Segmento data
    vaddr_t vbase2 = as->page_table->data->v_base;
    vaddr_t vtop2 = vbase2 + as->page_table->data->npages * PAGE_SIZE;

    if (vaddr >= vbase2 && vaddr < vtop2) {
        int index = (vaddr - vbase2) / PAGE_SIZE;
        return &as->page_table->data->entries[index];
    }

    // Segmento stack (crescita decrescente)
    vaddr_t stackbase = as->page_table->stack->v_base;
    vaddr_t stacktop = stackbase + as->page_table->stack->npages * PAGE_SIZE;

    if (vaddr >= stackbase && vaddr < stacktop) {
        int index = (vaddr - stacktop) / PAGE_SIZE;
        return &as->page_table->stack->entries[index];
    }

    // Nessun segmento trovato
    return NULL;
}
