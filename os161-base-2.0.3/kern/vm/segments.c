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

static int write_page(struct vnode *v, paddr_t paddr, int npage, uint8_t segment)
{

    Elf_Ehdr eh;   /* Executable header */
	Elf_Phdr ph;   /* "Program header" = segment header */
	int result;
	struct iovec iov;
	struct uio ku;
	struct addrspace *as;

	as = proc_getas();

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

    off_t offset = eh.e_phoff + segment*eh.e_phentsize;
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

	switch (ph.p_type) {
	    case PT_NULL: /* skip */ break;
	    case PT_PHDR: /* skip */ break;
	    case PT_MIPS_REGINFO: /* skip */ break;
	    case PT_LOAD: break;
	    default:
			kprintf("loadelf: unknown segment type %d\n",ph.p_type);
		return ENOEXEC;
	}

    offset = ph.p_offset + npage * PAGE_SIZE;
    vaddr_t vaddr = ph.p_vaddr + npage*PAGE_SIZE;

    size_t memsz = PAGE_SIZE;

    int npages = 0;
	KASSERT(segment == 0 || segment == 1 || segment == 2);
    switch(segment)
    {
        case 0: //code
            npages = as->page_table->code->npages;
            break;
        case 1: 
            npages = as->page_table->data->npages;
            break;
        case 2: 
            npages = as->page_table->stack->npages;
            break;
        default:
			break;

    }

    size_t filesz = PAGE_SIZE;
    if ((unsigned)npage == (unsigned)(npages-1) && ph.p_filesz < (unsigned)(npages*PAGE_SIZE))
    {
        size_t fragmentation = npages*PAGE_SIZE - ph.p_filesz;
        filesz = PAGE_SIZE - fragmentation;
    }

    //int is_executable = ph.p_flags & PF_X;


    
    if (filesz > memsz) {
		kprintf("ELF: warning: segment filesize > segment memsize\n");
		filesz = memsz;
	}

	DEBUG(DB_EXEC, "ELF: Loading %lu bytes to 0x%lx\n",
	      (unsigned long) filesz, (unsigned long) vaddr);

	
    iov.iov_kbase = (void *) PADDR_TO_KVADDR(paddr);
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
		/* short read; problem with executable? */
		kprintf("ELF: short read on segment - file truncated?\n");
		return ENOEXEC;
	}


    return 0;

}



int load_page(struct addrspace* as, int npage, paddr_t paddr, uint8_t segment)
{
    struct vnode *v;
	int result;

    char* progname = as->progname;

    result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

    result = write_page(v, paddr, npage, segment);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

    vfs_close(v);

    return 0;


}
