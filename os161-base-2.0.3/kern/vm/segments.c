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

static void zero_a_region(paddr_t paddr, size_t n)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), n);
}

static int write_page(struct vnode *v, paddr_t paddr, int npage, int segment)
{

    Elf_Ehdr eh;   /* Executable header */
	Elf_Phdr ph;   /* "Program header" = segment header */
	int result;
	struct iovec iov;
	struct uio ku;
	//struct addrspace *as;

	//as = proc_getas();

	/*
	 * Read the executable header from offset 0 in the file.
	 */

	uio_kinit(&iov, &ku, &eh, sizeof(eh), 0, UIO_READ);
	result = VOP_READ(v, &ku);
	if (result) {
		return result;
	}

	if (ku.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on header - file truncated?\n");
		return ENOEXEC;
	}

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
	    eh.e_type!=ET_EXEC ||
	    eh.e_machine!=EM_MACHINE) {
		return ENOEXEC;
	}

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

    off_t offset = eh.e_phoff + (segment+1)*eh.e_phentsize;
	uio_kinit(&iov, &ku, &ph, sizeof(ph), offset, UIO_READ);

	result = VOP_READ(v, &ku);
	if (result) {
		return result;
	}

	if (ku.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on phdr - file truncated?\n");
		return ENOEXEC;
	}

	// Calcola il numero massimo di pagine
    int max_file_pages = (ph.p_filesz + PAGE_SIZE - 1) / PAGE_SIZE;
    int max_mem_pages = (ph.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;

    // Controlla se la pagina richiesta supera la memoria del segmento
    if (npage >= max_mem_pages) {
        kprintf("ELF: Requested page %d exceeds memory size (%d pages)\n", npage, max_mem_pages);
        return ENOEXEC;
    }


    // Gestione delle pagine nella sezione .bss
    if (npage >= max_file_pages) {
        // Azzeramento della pagina .bss
        zero_a_region(paddr, PAGE_SIZE);
        return 0;
    }

    // Calcola offset e indirizzi per la lettura dal file ELF
    offset = ph.p_offset + npage * PAGE_SIZE;
    vaddr_t vaddr = ph.p_vaddr + npage * PAGE_SIZE;

    size_t memsz = PAGE_SIZE;
    size_t filesz = PAGE_SIZE;

    // Gestione della frammentazione nell'ultima pagina leggibile
    if ((unsigned)npage == (unsigned)(max_file_pages - 1) && ph.p_filesz < (unsigned)(max_file_pages * PAGE_SIZE)) {
        size_t fragmentation = max_file_pages * PAGE_SIZE - ph.p_filesz;
        filesz = PAGE_SIZE - fragmentation;
    }

    if (filesz > memsz) {
        kprintf("ELF: warning: segment filesize > segment memsize\n");
        filesz = memsz;
    }

    DEBUG(DB_EXEC, "ELF: Loading %lu bytes to 0x%lx\n", (unsigned long)filesz, (unsigned long)vaddr);

    // Lettura del contenuto del file nella memoria fisica
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
    if (result) {
        return result;
    }
    if (ku.uio_resid != 0) {
        kprintf("ELF: short read on segment - file truncated?\n");
        return ENOEXEC;
    }

    return 0;

}

int load_page(struct addrspace* as, int npage, paddr_t paddr, int segment)
{
	int result;

	zero_a_region(paddr,PAGE_SIZE);

    result = write_page(as->vfile, paddr, npage, segment);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(as->vfile);
		return result;
	}

    //vfs_close(v);

    return 0;
}
