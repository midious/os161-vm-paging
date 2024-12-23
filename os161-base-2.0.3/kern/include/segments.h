#ifndef _SEGMENTS_H_
#define _SEGMENTS_H_

#include <types.h>
#include <addrspace.h>

struct entry{

    paddr_t paddr; //indirizzo fisico corrispondente a offset zero della pagina desiderata
    bool valid_bit; //indica se la pagina è in memoria oppure no
    int swapIndex; //Se è uguale a -1, allora vuol dire che lo swap della pagina non è avvenuto
};

struct segment{

    struct entry* entries;
    vaddr_t v_base;
    size_t npages;
    bool readonly; //indica se il segmento è di sola lettura (segmento code)

};

struct addrspace;

int load_page(struct addrspace* as, int npage, paddr_t paddr, uint8_t segment);


#endif //_SEGMENTS_H_