#ifndef _SEGMENTS_H_
#define _SEGMENTS_H_

#include <types.h>

struct entry{

    paddr_t paddr; //indirizzo fisico corrispondente a offset zero della pagina desiderata
    bool valid_bit; //indica se la pagina è in memoria oppure no


};

struct segment{

    struct entry* entries;
    vaddr_t v_base;
    size_t npages;
    bool readonly; //indica se il segmento è di sola lettura (segmento code)

};



#endif //_SEGMENTS_H_