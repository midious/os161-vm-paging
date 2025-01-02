#include <segments.h>
#include <vfs.h>
#include <vm.h>
#include <addrspace.h>
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <vnode.h>
#include <elf.h>
#include <kern/fcntl.h>
#include <vmstats.h>

static void zero_a_region(paddr_t paddr, size_t n) {
    // Azzeramento della regione di memoria fisica a partire da paddr
    bzero((void *)PADDR_TO_KVADDR(paddr), n);
}

static int write_page(struct vnode *v, paddr_t paddr, int npage, int segment) {
    Elf_Ehdr eh;   // Header dell'eseguibile
    Elf_Phdr ph;   // Header del segmento (Program Header)
    int result;
    struct iovec iov;
    struct uio ku;

    // Leggi l'header dell'eseguibile
	/*
	 * Read the executable header from offset 0 in the file.
	 */
    uio_kinit(&iov, &ku, &eh, sizeof(eh), 0, UIO_READ);
    result = VOP_READ(v, &ku);
    if (result) return result;

    if (ku.uio_resid != 0) {
        kprintf("ELF: lettura incompleta sull'header - file troncato?\n");
        return ENOEXEC;
    }

    // Verifica che il file sia un eseguibile ELF valido
	/*
	 * Check to make sure it's a 32-bit ELF-version-1 executable
	 * for our processor type. If it's not, we can't run it.
	 *
	 * Ignore EI_OSABI and EI_ABIVERSION - properly, we should
	 * define our own, but that would require tinkering with the
	 * linker to have it emit our magic numbers instead of the
	 * default ones. (If the linker even supports these fields,
	 * which were not in the original elf spec.)
	 */
    if (eh.e_ident[EI_MAG0] != ELFMAG0 ||
        eh.e_ident[EI_MAG1] != ELFMAG1 ||
        eh.e_ident[EI_MAG2] != ELFMAG2 ||
        eh.e_ident[EI_MAG3] != ELFMAG3 ||
        eh.e_ident[EI_CLASS] != ELFCLASS32 ||
        eh.e_ident[EI_DATA] != ELFDATA2MSB ||
        eh.e_ident[EI_VERSION] != EV_CURRENT ||
        eh.e_version != EV_CURRENT ||
        eh.e_type != ET_EXEC ||
        eh.e_machine != EM_MACHINE) {
        return ENOEXEC;
    }

    // Leggi l'header del segmento (Program Header)
	/*
	 * Go through the list of segments and set up the address space.
	 *
	 * Ordinarily there will be one code segment, one read-only
	 * data segment, and one data/bss segment, but there might
	 * conceivably be more. You don't need to support such files
	 * if it's unduly awkward to do so.
	 *
	 * Note that the expression eh.e_phoff + i*eh.e_phentsize is
	 * mandated by the ELF standard - we use sizeof(ph) to load,
	 * because that's the structure we know, but the file on disk
	 * might have a larger structure, so we must use e_phentsize
	 * to find where the phdr starts.
	 */
    off_t offset = eh.e_phoff + (segment + 1) * eh.e_phentsize;
    uio_kinit(&iov, &ku, &ph, sizeof(ph), offset, UIO_READ);
    result = VOP_READ(v, &ku);
    if (result) return result;

    if (ku.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on phdr - file truncated?\n");
		return ENOEXEC;
	}

    // Calcola il numero massimo di pagine per il segmento
    int max_file_pages = (ph.p_filesz + PAGE_SIZE - 1) / PAGE_SIZE;
    int max_mem_pages = (ph.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;

    // Verifica se la pagina richiesta supera la memoria del segmento
    if (npage >= max_mem_pages) {
        kprintf("ELF: Requested page %d exceeds memory size (%d pages)\n", npage, max_mem_pages);
        return ENOEXEC;
    }

    // Gestione delle pagine .bss (memoria zero-inizializzata)
    if (npage >= max_file_pages) {
        zero_a_region(paddr, PAGE_SIZE);  // Azzeramento della pagina .bss
		vmstats_increment(PAGE_FAULTS_ZEROED);
        return 0;
    }

    // Gestione delle dimensioni delle pagine del file e della memoria
    offset = ph.p_offset + npage * PAGE_SIZE;
    size_t memsz = PAGE_SIZE;
    size_t filesz = PAGE_SIZE;

    // Gestione della frammentazione nell'ultima pagina leggibile
    if (npage == max_file_pages - 1 && ph.p_filesz < (unsigned)(max_file_pages * PAGE_SIZE)) {
        size_t fragmentation = max_file_pages * PAGE_SIZE - ph.p_filesz;
        filesz = PAGE_SIZE - fragmentation;
    }

    // Regola la dimensione della memoria se necessario
    if (filesz > memsz) {
        kprintf("ELF: warning: segment filesize > segment memsize\n");
        filesz = memsz;
    }

    // Regola l'offset per la prima pagina del segmento
    if (npage == 0 && (offset % PAGE_SIZE) > 0) {
        paddr = paddr + (offset % PAGE_SIZE);
        if (PAGE_SIZE - (offset % PAGE_SIZE) < filesz) {
            memsz = memsz - (offset % PAGE_SIZE);
            offset = offset % PAGE_SIZE;
        } else {
            memsz = filesz;
        }
    }

    // Leggi il contenuto del segmento nel buffer di memoria fisica
    iov.iov_kbase = (void *)PADDR_TO_KVADDR(paddr);
    iov.iov_len = memsz;
    ku.uio_iov = &iov;
    ku.uio_iovcnt = 1;
    ku.uio_offset = offset;
    ku.uio_resid = filesz;
    ku.uio_segflg = UIO_SYSSPACE;
    ku.uio_rw = UIO_READ;
    ku.uio_space = NULL;

    result = VOP_READ(v, &ku);
    if (result) return result;
    if (ku.uio_resid != 0) {
        kprintf("ELF: short read on segment - file truncated?\n");
        return ENOEXEC;
    }

	vmstats_increment(PAGE_FAULTS_ELF);
	vmstats_increment(PAGE_FAULTS_DISK);

    return 0;
}

int load_page(struct addrspace* as, int npage, paddr_t paddr, int segment) {
    int result;

    zero_a_region(paddr, PAGE_SIZE);  // Azzeramento della pagina
    result = write_page(as->vfile, paddr, npage, segment);  // Scrittura della pagina nel segmento
    if (result) {
        vfs_close(as->vfile);  // Chiusura del file se c'è stato un errore
        return result;
    }

    return 0;
}
