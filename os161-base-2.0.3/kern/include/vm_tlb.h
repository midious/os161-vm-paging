#ifndef _VMTLB_H_
#define _VMTLB_H_

#include <types.h>

void tlb_insert(vaddr_t vaddr, paddr_t paddr, uint8_t readonly);
void tlb_invalid(void);

#endif