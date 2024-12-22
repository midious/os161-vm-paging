#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <addrspace.h>

/*
 * Structure used to keep track of the state of all memory pages.
 */
struct coremap_entry {
    bool occupied;       // Defines the state of the page 1=occupied  0=free
    bool freed;         //Indica se la entry è stata liberata (utile per la getfreepages) freed=1 è stata liberata freed=0 non è stata liberata (sarà occupata o untracked)
   
    int allocSize; //quante pagine a partire dalla pagina corrente sono allocate

    //linked list interna per coda delle pagine user
    int prevAllocated;
    int nextAllocated;
    //solo per User
    struct addrspace *as; //addrespace della pagina richiesta
    vaddr_t vaddr; //indirizzo d'inizio della pagina richiesta
};

void coremap_init(void);
void coremap_shutdown(void);
vaddr_t alloc_kpages(size_t npages);
void free_kpages(vaddr_t addr);
paddr_t alloc_upage(vaddr_t vaddr);
void freeppage_user(paddr_t paddr);

#endif