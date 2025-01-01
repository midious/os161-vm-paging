#ifndef _VMSTATS_H_
#define _VMSTATS_H_

#include <types.h>
#include <addrspace.h>

#define TLB_FAULTS                  0 // The number of TLB misses that have occurred
#define TLB_FAULTS_WITH_FREE        1 // The number of TLB misses for which there was free space in the TLB to add the new TLB entry 
#define TLB_FAULTS_WITH_REPLACE     2 // The number of TLB misses for which there was no free space for the new TLB entry, 
                                        // so replacement was required.
#define TLB_INVALIDATIONS           3 // The number of times the TLB was invalidated
#define TLB_RELOADS                 4 // The number of TLB misses for pages that were already in memory
#define PAGE_FAULTS_ZEROED          5 // The number of TLB misses that required a new page to be zero-filled
#define PAGE_FAULTS_DISK            6 // The number of TLB misses that required a page to be loaded from disk
#define PAGE_FAULTS_ELF             7 // The number of page faults that require getting a page from the ELF file
#define PAGE_FAULTS_SWAP            8 // The number of page faults that require getting a page from the swap file
#define SWAPFILE_WRITES             9 // The number of page faults that require writing a page to the swap file

struct statistics{
    unsigned int tlb_faults;
    unsigned int tlb_faults_with_free;
    unsigned int tlb_faults_with_replace;
    unsigned int tlb_invalidations;
    unsigned int tlb_reloads;
    unsigned int page_faults_zeroed;
    unsigned int page_faults_disk;
    unsigned int page_faults_elf;
    unsigned int page_faults_swap;
    unsigned int swapfile_writes;
};

void vmstats_init(void);
void vmstats_increment(int code);
void vmstats_shutdown(void);



#endif