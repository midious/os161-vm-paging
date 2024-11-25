#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <addrspace.h>

/*
 * Structure used to keep track of the state of all memory pages.
 */
struct coremap_entry {
    unsigned bool occupied;       // Defines the state of the page 1= occupied  0=free
    //altri campi
};

vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);