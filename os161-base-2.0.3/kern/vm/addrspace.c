/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
#include <coremap.h>
#include <vm_tlb.h>
#include <swapfile.h>
#include <vmstats.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */


#define PAGING_STACKPAGES    18

//static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

//static struct spinlock freemem_lock = SPINLOCK_INITIALIZER;


void
vm_bootstrap(void)
{

	//inizializza stats, coremap e swap

	coremap_init();
	swapfile_init();
	vmstats_init();

}

void vm_shutdown(void)
{
	coremap_shutdown();
	swap_shutdown();
	vmstats_shutdown();
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}
/*
static void print_tlb_contents() {
    uint32_t entryhi, entrylo;
    int i;

    // Itera su tutte le voci della TLB
    for (i = 0; i < NUM_TLB; i++) {
        tlb_read(&entryhi, &entrylo, i);  // Legge la voce della TLB all'indice i
		
		if(entrylo & TLBLO_VALID){
			DEBUG(DB_VM,"valido");
			DEBUG(DB_VM,"valido");
		}else{
			DEBUG(DB_VM,"non valido");
			DEBUG(DB_VM,"non valido secondo");
		}
        
    }
}
*/


int vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	struct addrspace *as;
	int index_page_table;
	int result;


	faultaddress &= PAGE_FRAME; //indirizzo logico (pagina) in cui avviene il tlb fault

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n"); // TODO: NON DEVE ANDARE IN PANIC !!!!!!!!!!!!! Deve solo terminare il processo 
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}


	KASSERT(as->page_table != NULL);

	KASSERT(as->page_table->code != NULL);
	KASSERT(as->page_table->data != NULL);
	KASSERT(as->page_table->stack != NULL);

	KASSERT(as->page_table->code->v_base!=0);
	KASSERT(as->page_table->code->npages!=0);
	KASSERT(as->page_table->code->entries!=NULL);

	KASSERT(as->page_table->data->v_base!=0);
	KASSERT(as->page_table->data->npages!=0);
	KASSERT(as->page_table->data->entries!=NULL);

	KASSERT(as->page_table->stack->v_base!=0);
	KASSERT(as->page_table->stack->npages!=0);
	KASSERT(as->page_table->stack->entries!=NULL);

	//gli indirizzi logici di partenza dei segmenti devono essere allineati alla pagina
	KASSERT((as->page_table->code->v_base & PAGE_FRAME) == as->page_table->code->v_base);
	KASSERT((as->page_table->data->v_base & PAGE_FRAME) == as->page_table->data->v_base);
	KASSERT((as->page_table->stack->v_base & PAGE_FRAME) == as->page_table->stack->v_base);

	//Cercare nella ram l'indirizzo fisico fisico corrispondente all'indirizzo logico del faultaddress e CONTROLLARE VALID BIT.
	//Se non è valida la entry significa che non è in memoria perché non è stato caricato ancora oppure la pagina è stata sostituita.
	//Se è presente in memoria carico semplicemente nella TLB.
	//Se non è presente, devo cercare un frame libero

	//devo capire in quale segmento fa parte il faultaddress. 


	//incremento tlb_faults
	vmstats_increment(TLB_FAULTS);

	vbase1 = as->page_table->code->v_base;
	vtop1 = vbase1 + as->page_table->code->npages * PAGE_SIZE;

	vbase2 = as->page_table->data->v_base;
	vtop2 = vbase2 + as->page_table->data->npages * PAGE_SIZE;

	stackbase = as->page_table->stack->v_base;//ricorda: lo stack cresce scendendo con l'indirizzo logico!
	stacktop = stackbase + as->page_table->stack->npages * PAGE_SIZE;

	if (faultaddress >= vbase1 && faultaddress < vtop1) { //l'indirizzo faultaddress è contenuto dentro il segmento di code

		// accedo direttamente a as->page_table->code->entries trovando l'indice di tale vettore
		
		index_page_table = (int) (faultaddress - vbase1)/PAGE_SIZE;
		if(as->page_table->code->entries[index_page_table].valid_bit == 0)
		{
			//Non è in memoria perché il valid bit della page table è uguale a 0

			paddr = alloc_upage(faultaddress); //gestisce anche un eventuale swap out per liberare un frame
									//se non è possibile fare neanche swap, allora termina il processo (system call)
									//alloc upages gestisce anche la modifica della coremap e l'eventuale swap
			

			KASSERT((paddr & PAGE_FRAME) == paddr);

			as->page_table->code->entries[index_page_table].valid_bit = 1; // convalido la pagina
			as->page_table->code->entries[index_page_table].paddr = paddr;

			

			if(as->page_table->code->entries[index_page_table].swapIndex == -1)
			{
				//niente swap: non è nello swapfile
				result = load_page(as, index_page_table, paddr, 0); 
				if (result)
				{
					return result;
				}

			
			}
			else
			{
				//SWAP IN -code
				swapin(as->page_table->code->entries[index_page_table].swapIndex,paddr);
				vmstats_increment(PAGE_FAULTS_DISK);
				vmstats_increment(PAGE_FAULTS_SWAP);
			}

			tlb_insert(faultaddress, paddr, 1);



		}
		else
		{
			

			paddr = as->page_table->code->entries[index_page_table].paddr;

			KASSERT((paddr & PAGE_FRAME) == paddr); // deve avere gli ultimi bit (dell'offset) uguali a 0

			tlb_insert(faultaddress, paddr, 1); //l'1 indica che è readonly, quindi il dirty bit della tlb sarà settato a 0

			vmstats_increment(TLB_RELOADS);

		}

	}
	else
		if (faultaddress >= vbase2 && faultaddress < vtop2) { //l'indirizzo faultaddress è contenuto dentro il segmento di code

			// accedo direttamente a as->page_table->data->entries trovando l'indice di tale vettore
			
			index_page_table = (int) (faultaddress - vbase2)/PAGE_SIZE;
			if(as->page_table->data->entries[index_page_table].valid_bit == 0)
			{
				//Non è in memoria perché il valid bit della page table è uguale a 0

				paddr = alloc_upage(faultaddress); //se non è possibile fare neanche swap, allora termina il processo (system call)
									//alloc upages gestisce anche la modifica della coremap e l'eventuale swap
			

				KASSERT((paddr & PAGE_FRAME) == paddr);

				as->page_table->data->entries[index_page_table].valid_bit = 1; // convalido la pagina
				as->page_table->data->entries[index_page_table].paddr = paddr;


				if(as->page_table->data->entries[index_page_table].swapIndex == -1)
				{
					//niente swap: non è nello swapfile
					result = load_page(as, index_page_table, paddr, 1); 
					if (result)
					{
						return result;
					}
				
				}
				else
				{
					//SWAP IN - data
					swapin(as->page_table->data->entries[index_page_table].swapIndex,paddr);
					vmstats_increment(PAGE_FAULTS_DISK);
					vmstats_increment(PAGE_FAULTS_SWAP);
				}

				tlb_insert(faultaddress, paddr, 0);
				
			}
			else
			{
				paddr = as->page_table->data->entries[index_page_table].paddr;

				KASSERT((paddr & PAGE_FRAME) == paddr); // deve avere gli ultimi bit (dell'offset) uguali a 0

				tlb_insert(faultaddress, paddr, 0);

				vmstats_increment(TLB_RELOADS);
			}

		}
		else
			if (faultaddress >= stackbase && faultaddress < stacktop){
				
				
				//Attenzione: lo stack è decrescente, E ANCHE nella page table è decrescente.


				index_page_table = (faultaddress - stackbase) / PAGE_SIZE;
				if(as->page_table->stack->entries[index_page_table].valid_bit == 0)
				{
					
					//Non è in memoria perché il valid bit della page table è uguale a 0

					paddr = alloc_upage(faultaddress); //se non è possibile fare neanche swap, allora termina il processo (system call)
										//alloc upages gestisce anche la modifica della coremap e l'eventuale swap
				
					

					KASSERT((paddr & PAGE_FRAME) == paddr);

					as->page_table->stack->entries[index_page_table].valid_bit = 1; // convalido la pagina
					as->page_table->stack->entries[index_page_table].paddr = paddr;



					if(as->page_table->stack->entries[index_page_table].swapIndex != -1)
					{
						//è necessario fare lo swap in
						//TODO: qui non c'è il "load_page" se swapIndex==1?
						swapin(as->page_table->stack->entries[index_page_table].swapIndex,paddr);
						vmstats_increment(PAGE_FAULTS_DISK);
						vmstats_increment(PAGE_FAULTS_SWAP);
					}
					else
					{
						bzero((void* ) PADDR_TO_KVADDR(paddr), PAGE_SIZE);
						vmstats_increment(PAGE_FAULTS_ZEROED);
					}
					
					tlb_insert(faultaddress, paddr, 0);

				}
				else
				{
					paddr = as->page_table->stack->entries[index_page_table].paddr;

					KASSERT((paddr & PAGE_FRAME) == paddr); // deve avere gli ultimi bit (dell'offset) uguali a 0

					tlb_insert(faultaddress, paddr, 0);

					vmstats_increment(TLB_RELOADS);
				}
			}
			else
			{
				return EFAULT;
			}
	return 0;
}



struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	as->page_table = kmalloc(sizeof(struct pt));
	if (as->page_table == NULL)
	{
		kfree(as);
		return NULL;
	}


	as->page_table->code = kmalloc(sizeof(struct segment));
	if(as->page_table->code == NULL)
	{
		kfree(as->page_table);
		kfree(as);
		return NULL;
	}

	as->page_table->code->entries = NULL;
	as->page_table->code->v_base = 0;
	as->page_table->code->npages = 0;
	as->page_table->code->readonly = 1;

	as->page_table->data = kmalloc(sizeof(struct segment));
	if(as->page_table->data == NULL)
	{
		kfree(as->page_table->code);
		kfree(as->page_table);
		kfree(as);
		return NULL;
	}

	as->page_table->data->entries = NULL;
	as->page_table->data->v_base = 0;
	as->page_table->data->npages = 0;
	as->page_table->data->readonly = 0;

	as->page_table->stack = kmalloc(sizeof(struct segment));
	if(as->page_table->stack == NULL)
	{
		kfree(as->page_table->code);
		kfree(as->page_table->data);
		kfree(as->page_table);
		kfree(as);
		return NULL;
	}

	as->page_table->stack->entries = NULL;
	as->page_table->stack->v_base = 0;
	as->page_table->stack->npages = 0;
	as->page_table->stack->readonly = 0;

	/*

	as->page_table->code->entries = kmalloc(sizeof(struct entry));
	if(as->page_table->code->entries == NULL)
	{
		return NULL;
	}

	as->page_table->data->entries = kmalloc(sizeof(struct entry));
	if(as->page_table->data->entries == NULL)
	{
		return NULL;
	}

	as->page_table->stack->entries = kmalloc(sizeof(struct entry));
	if(as->page_table->stack->entries == NULL)
	{
		return NULL;
	}

	*/

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{

	unsigned int i;
	/*
	 * Clean up as needed.
	 */

	can_sleep();

	//libera il segmento code

	for(i = 0; i < as->page_table->code->npages; i++)
	{
		if(as->page_table->code->entries[i].valid_bit == 1)
		{
			freeppage_user(as->page_table->code->entries[i].paddr);
		}
		else
		{
			if(as->page_table->code->entries[i].swapIndex != -1)
			{
				//non è in memoria ma nello swap file
				swap_free(as->page_table->code->entries[i].swapIndex);
			}
		}
	}

	kfree(as->page_table->code->entries);
	kfree(as->page_table->code);


	//libera il segmento data
	for(i = 0; i < as->page_table->data->npages; i++)
	{
		if(as->page_table->data->entries[i].valid_bit == 1)
		{
			freeppage_user(as->page_table->data->entries[i].paddr);
		}
		else
		{
			if(as->page_table->data->entries[i].swapIndex != -1)
			{
				//non è in memoria ma nello swap file
				swap_free(as->page_table->data->entries[i].swapIndex);
			}
		}
	}

	kfree(as->page_table->data->entries);
	kfree(as->page_table->data);



	//libera il segmento di stack


	for(i = 0; i < as->page_table->stack->npages; i++)
	{
		if(as->page_table->stack->entries[i].valid_bit == 1)
		{
			freeppage_user(as->page_table->stack->entries[i].paddr);
		}
		else
		{
			if(as->page_table->stack->entries[i].swapIndex != -1)
			{
				//non è in memoria ma nello swap file
				swap_free(as->page_table->stack->entries[i].swapIndex);
			}
		}
	}

	invalidVictim();

	kfree(as->page_table->stack->entries);
	kfree(as->page_table->stack);


	kfree(as->page_table);
	vfs_close(as->vfile);
	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	//rivedi per bene. Dovrebbe essere giusto così

	tlb_invalid();

	vmstats_increment(TLB_INVALIDATIONS);

	
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;
	int i;

	can_sleep();

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;


	if (as->page_table->code->v_base == 0) {
		as->page_table->code->v_base = vaddr;
		as->page_table->code->npages = npages;
		as->page_table->code->entries = kmalloc(npages*sizeof(struct entry));

		if(as->page_table->code->entries == NULL)
		{
			kfree(as->page_table->code);
			kfree(as->page_table->data);
			kfree(as->page_table->stack);
			kfree(as->page_table);
			kfree(as);

			return ENOMEM;
		} 

		for(i = 0; i<(int)npages; i++)
		{
			as->page_table->code->entries[i].valid_bit = 0;
			as->page_table->code->entries[i].paddr = 0;
			as->page_table->code->entries[i].swapIndex = -1;
		}

		return 0;
	}

	if (as->page_table->data->v_base == 0) {
		as->page_table->data->v_base = vaddr;
		as->page_table->data->npages = npages;
		as->page_table->data->entries = kmalloc(npages*sizeof(struct entry));

		if(as->page_table->data->entries == NULL)
		{
			kfree(as->page_table->code);
			kfree(as->page_table->data);
			kfree(as->page_table->stack);
			kfree(as->page_table);
			kfree(as);

			return ENOMEM;
		} 

		for(i = 0; i<(int)npages; i++)
		{
			as->page_table->data->entries[i].valid_bit = 0;
			as->page_table->data->entries[i].paddr = 0;
			as->page_table->data->entries[i].swapIndex = -1;
		}



		return 0;
	}
	


	return ENOSYS;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	can_sleep();
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	as->page_table->stack->v_base = USERSTACK - PAGING_STACKPAGES * PAGE_SIZE;
	as->page_table->stack->npages = PAGING_STACKPAGES;
	as->page_table->stack->entries = kmalloc(PAGING_STACKPAGES * sizeof(struct entry));

	if (as->page_table->stack->entries == NULL)
	{
		kfree(as->page_table->code);
		kfree(as->page_table->data);
		kfree(as->page_table->stack);
		kfree(as->page_table);
		kfree(as);

		return ENOMEM;	
	}

	for(int i = 0; i<PAGING_STACKPAGES; i++)
	{
		as->page_table->stack->entries[i].valid_bit = 0;
		as->page_table->stack->entries[i].paddr = 0;
		as->page_table->stack->entries[i].swapIndex = -1;

	}


	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

