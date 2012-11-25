/*
 * Code to load an ELF-format executable into the current address space.
 *
 * Right now it just copies into userspace and hopes the addresses are
 * mappable to real memory. This works with dumbvm; however, when you
 * write a real VM system, you will need to either (1) add code that 
 * makes the address range used for load valid, or (2) if you implement
 * memory-mapped files, map each segment instead of copying it into RAM.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <uio.h>
#include <elf.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vnode.h>

/*
 * Load a segment at virtual address VADDR. The segment in memory
 * extends from VADDR up to (but not including) VADDR+MEMSIZE. The
 * segment on disk is located at file offset OFFSET and has length
 * FILESIZE.
 *
 * FILESIZE may be less than MEMSIZE; if so the remaining portion of
 * the in-memory segment should be zero-filled.
 *
 * Note that uiomove will catch it if someone tries to load an
 * executable whose load address is in kernel space. If you should
 * change this code to not use uiomove, be sure to check for this case
 * explicitly.
 */
static
int
load_segment(struct vnode *v, off_t offset, vaddr_t vaddr, 
	     size_t memsize, size_t filesize,
	     int is_executable)
{
	struct uio u;
	int result;
	size_t fillamt;

	if (filesize > memsize) {
		kprintf("ELF: warning: segment filesize > segment memsize\n");
		filesize = memsize;
	}

	DEBUG(DB_EXEC, "ELF: Loading %lu bytes to 0x%lx\n", 
	      (unsigned long) filesize, (unsigned long) vaddr);

	u.uio_iovec.iov_ubase = (userptr_t)vaddr;
	u.uio_iovec.iov_len = memsize;   // length of the memory space
	u.uio_resid = filesize;          // amount to actually read
	u.uio_offset = offset;
	u.uio_segflg = is_executable ? UIO_USERISPACE : UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curthread->t_vmspace;

	result = VOP_READ(v, &u);
	if (result) {
		return result;
	}

	if (u.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on segment - file truncated?\n");
		return ENOEXEC;
	}

	/* Fill the rest of the memory space (if any) with zeros */
	fillamt = memsize - filesize;
	if (fillamt > 0) {
		DEBUG(DB_EXEC, "ELF: Zero-filling %lu more bytes\n", 
		      (unsigned long) fillamt);
		u.uio_resid += fillamt;
		result = uiomovezeros(fillamt, &u);
	}
	
	return result;
}

/*
 * Load an ELF executable user program into the current address space.
 *
 * Returns the entry point (initial PC) for the program in ENTRYPOINT.
 */
int
load_elf(struct vnode *v, vaddr_t *entrypoint)
{
	Elf_Ehdr eh;   /* Executable header */
	Elf_Phdr ph;   /* "Program header" = segment header */
	int result, i;
	struct uio ku;

	/*
	 * Read the executable header from offset 0 in the file.
	 */

	mk_kuio(&ku, &eh, sizeof(eh), 0, UIO_READ);
	result = VOP_READ(v, &ku);
	if (result) {
		return result;
	}

	if (ku.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on header - file truncated?\n");
		return ENOEXEC;
	}

    result = as_define_eh(curthread->t_vmspace, v, &eh);
    if (result) {
        return result;
    }

	result = as_prepare_load(curthread->t_vmspace);
	if (result) {
		return result;
	}

	/*
	 * Now actually load each segment.
	 */

	// for (i=0; i<eh.e_phnum; i++) {
		// off_t offset = eh.e_phoff + i*eh.e_phentsize;
		// mk_kuio(&ku, &ph, sizeof(ph), offset, UIO_READ);

		// result = VOP_READ(v, &ku);
		// if (result) {
			// return result;
		// }

		// if (ku.uio_resid != 0) {
			// /* short read; problem with executable? */
			// kprintf("ELF: short read on phdr - file truncated?\n");
			// return ENOEXEC;
		// }

		// switch (ph.p_type) {
		    // case PT_NULL: /* skip */ continue;
		    // case PT_PHDR: /* skip */ continue;
		    // case PT_MIPS_REGINFO: /* skip */ continue;
		    // case PT_LOAD: break;
		    // default:
			// kprintf("loadelf: unknown segment type %d\n", 
				// ph.p_type);
			// return ENOEXEC;
		// }

		// result = load_segment(v, ph.p_offset, ph.p_vaddr, 
				      // ph.p_memsz, ph.p_filesz,
				      // ph.p_flags & PF_X);
		// if (result) {
			// return result;
		// }
	// }

	// result = as_complete_load(curthread->t_vmspace);
	// if (result) {
		// return result;
	// }

	*entrypoint = eh.e_entry;

	return 0;
}
