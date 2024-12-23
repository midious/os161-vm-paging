#include <types.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <vm.h>
#include <swapfile.h>
#include <bitmap.h>
//#include <vmstats.h>

static struct spinlock swap_lock = SPINLOCK_INITIALIZER;

//Virtual node - astrazione del file Swapfile - il kernel interagisce con open read write.
//Sarà creato all'inizializzazione della VM (in emu0)
static struct vnode *swapfile;

//Tenere conto di quale pagine dello swapfile sono occupate
static struct bitmap *swapfilemap;

int swapfile_init(){
    int open;
    char path[32];

    strcpy(path, SWAPFILE_PATH);
    open = vfs_open(path, O_RDWR | O_CREAT, 0, &swapfile);
    if (open) {
        panic("swapfile.c : Impossibile aprire lo swapfile\n");
    }

    swapfilemap= bitmap_create(SWAPFILE_SIZE/PAGE_SIZE);
    return 0;
}

//Scrive all'interno dello swapfile la pagina contenuta all'indirizzo di memoria paddr e ritorna l' indice (swapIndex) di dove è stato salvato all'interno del file
int swapout(paddr_t paddr ){ //"dalla ram allo swapfile (disco)"
    struct iovec iov;
    struct uio u;
    int result;
    unsigned int index; //indice della bitmap dove verrà salvato
    off_t free_offset;

    KASSERT(paddr != 0);
    KASSERT((paddr & PAGE_FRAME) == paddr);

    spinlock_acquire(&swap_lock);
    result = bitmap_alloc(swapfilemap, &index);
    if (result) {
        panic("swapfile.c : Non c'è abbastanza spazio nello swapfile\n");
    }
    spinlock_release(&swap_lock);

    free_offset=index*PAGE_SIZE;

    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, free_offset, UIO_WRITE);
    VOP_WRITE(swapfile, &u);
    if (u.uio_resid != 0) { //controllo: indica quanti byte della scrittura non sono stati trasferiti
        panic("swapfile.c : Impossibile scrivere sullo swapfile\n");
    }

    KASSERT(index<(SWAPFILE_SIZE/PAGE_SIZE));

    return index;
}

int swapin(int swapIndex, paddr_t paddr){ //"dallo swapfile alla ram"
    off_t offset;
    struct iovec iov;
    struct uio u;

    offset=swapIndex*PAGE_SIZE;

    KASSERT((paddr & PAGE_FRAME) == paddr);
    KASSERT((offset & PAGE_FRAME) == offset);
    KASSERT(offset < SWAPFILE_SIZE);

    spinlock_acquire(&swap_lock);
    if (!bitmap_isset(swapfilemap, swapIndex)) { //controlla se il bit è a 1
        panic("swapfile.c: Si sta provando ad accedere ad una pagina non riempita dello swapfile\n");
    }
    spinlock_release(&swap_lock);

    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, offset, UIO_READ);
    VOP_READ(swapfile, &u);
    if (u.uio_resid != 0) {
        panic("swapfile.c : Impossibile leggere dallo swapfile all'indirizzo %u\n", paddr);
    }

    spinlock_acquire(&swap_lock);
    bitmap_unmark(swapfilemap, swapIndex); //setta a 0 il bit
    spinlock_release(&swap_lock);

    return 0;
}

void swap_free(int indexSwap) {
    off_t offset;

    offset=indexSwap*PAGE_SIZE;

    KASSERT((offset & PAGE_FRAME) == offset);
    KASSERT(offset < SWAPFILE_SIZE);

    spinlock_acquire(&swap_lock);
    if (!bitmap_isset(swapfilemap, indexSwap)) {
        panic("swapfile.c: Errore:Impossibile libera pagina dello swapfile già vuota\n");
    }

    //setta il bit a 0
    bitmap_unmark(swapfilemap, indexSwap);
    spinlock_release(&swap_lock);
}

void swap_shutdown(void) {
    KASSERT(swapfile != NULL);
    KASSERT(swapfilemap != NULL);

    vfs_close(swapfile);
    bitmap_destroy(swapfilemap);
}