#ifndef _PT_H_
#define _PT_H_

#include <segments.h>


struct pt{

    struct segment* code;
    struct segment* data;
    struct segment* stack;
};

struct addrspace;

struct entry* get_pt_entry(vaddr_t vaddr, struct addrspace *as);



#endif //_PT_H_