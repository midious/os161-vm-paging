#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <addrspace.h>

/*
 * Structure used to keep track of the state of all memory pages.
 */
struct coremap_entry {
    bool occupied;       // Defines the state of the page 1=occupied  0=free
    //per il kernerl
    unsigned long allocSize; //quante pagine a partire dalla pagina corrente sono allocate
    unsigned long prevAllocated;
    unsigned long nextAllocated;
    //in più per l'user
    struct addrspace *as; //addrespace della pagina richiesta
    vaddr_t vaddr; //indirizzo d'inizio della pagina richiesta
};

void coremap_init(void);
void coremap_shutdown(void);
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);

#endif