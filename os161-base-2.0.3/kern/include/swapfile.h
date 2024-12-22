#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#define SWAPFILE_SIZE 9*1024*1024
#define SWAPFILE_PATH "emu0:/SWAPFILE"


int swapfile_init(void);
int swapout(paddr_t paddr );
int swapin(int swapIndex, paddr_t paddr);
void swap_free(int indexSwap);
void swap_shutdown(void);

#endif 