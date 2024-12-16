#ifndef _PT_H_
#define _PT_H_

#include <segments.h>


struct pt{

    struct segment* code;
    struct segment* data;
    struct segment* stack;
};



#endif //_PT_H_