#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_

#include <vm.h>
// #include "opt-dumbvm.h"

struct vnode;

/* 
 * Address space - data structure associated with the virtual memory
 * space of a process.
 *
 * You write this.
 */
typedef struct {
    paddr_t    frameAddr: 20;
    u_int32_t writable: 1;
    u_int32_t valid: 1;
    u_int32_t global: 1; // ignore pid bits if set
    u_int32_t unused: 8;
} PageTableEntry;

typedef struct {
    PageTableEntry pte[1024];
} PageTableL2;

typedef struct {
    PageTableL2*    pageTableL2[512];
} PageTableL1;

typedef struct {
    vaddr_t regionBase: 20; // virtual frame address 4k alignment
    u_int32_t regionLength: 12; // size of the region
    u_int32_t regionType;
} Region;

#define REGION_TEXT 1
#define REGION_DATA 2
#define REGION_STACK 3
#define REGION_HEAP 4

struct addrspace {
	vaddr_t as_vbase1;
	// paddr_t as_pbase1;
	size_t as_npages1;
	vaddr_t as_vbase2;
	// paddr_t as_pbase2;
	size_t as_npages2;
	paddr_t as_stackvbase;
    PageTableL1 pageTable;
};

/*
 * Functions in addrspace.c:
 *
 *    as_create - create a new empty address space. You need to make 
 *                sure this gets called in all the right places. You
 *                may find you want to change the argument list. May
 *                return NULL on out-of-memory error.
 *
 *    as_copy   - create a new address space that is an exact copy of
 *                an old one. Probably calls as_create to get a new
 *                empty address space and fill it in, but that's up to
 *                you.
 *
 *    as_activate - make the specified address space the one currently
 *                "seen" by the processor. Argument might be NULL, 
 *		  meaning "no particular address space".
 *
 *    as_destroy - dispose of an address space. You may need to change
 *                the way this works if implementing user-level threads.
 *
 *    as_define_region - set up a region of memory within the address
 *                space.
 *
 *    as_prepare_load - this is called before actually loading from an
 *                executable into the address space.
 *
 *    as_complete_load - this is called when loading from an executable
 *                is complete.
 *
 *    as_define_stack - set up the stack region in the address space.
 *                (Normally called *after* as_complete_load().) Hands
 *                back the initial stack pointer for the new process.
 */

struct addrspace *as_create(void);
int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_activate(struct addrspace *);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as, 
				   vaddr_t vaddr, size_t sz,
				   int readable, 
				   int writeable,
				   int executable);
int		  as_prepare_load(struct addrspace *as);
int		  as_complete_load(struct addrspace *as);
int               as_define_stack(struct addrspace *as, vaddr_t *initstackptr);

/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */

int load_elf(struct vnode *v, vaddr_t *entrypoint);


#endif /* _ADDRSPACE_H_ */
