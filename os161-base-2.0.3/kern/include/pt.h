#ifndef _PT_H_
#define _PT_H_

#include <segments.h>


struct pt{

    struct segment* code;
    struct segment* data;
    struct segment* stack;
};

//TODO: DA CORREGGERE
struct addrspace; // per ovviare all'errore che non è ancora stato definito "struct addrspace", probabilmente qualche errore di dipendenze

struct entry* get_pt_entry(vaddr_t vaddr, struct addrspace *as);



#endif //_PT_H_