#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <coremap.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

void
vm_bootstrap(void)
{
	/* Do nothing. */
}

static
paddr_t
getppages(unsigned long npages)
{
	int spl;
	paddr_t addr;

	spl = splhigh();
    
	addr = GetNFreePage(npages);
    // kprintf("getpages.\n");
	if (addr) 
        AllocateNPages(addr, 1, npages);
    CoreMapReport();
	splx(spl);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
    int spl = splhigh();
    FreeNPages(KVADDR_TO_PADDR(addr));
    CoreMapReport();
	splx(spl);
}

int GetPhysicalFrame(PageTableL1 *ptl1, vaddr_t vaddr, paddr_t *paddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    assert(paddr != NULL);
    
    vaddr = (unsigned)vaddr >> 12;
    u_int32_t pageTableL1Index = vaddr >> 10;
    u_int32_t pageTableL2Index = vaddr & 0x3ff;
    
    PageTableL2* pageTableL2 = ptl1->pageTableL2[pageTableL1Index];
    if (pageTableL2 == NULL) {
        return -1;
    }
    PageTableEntry *pte = &(pageTableL2->pte[pageTableL2Index]);
    if (pte->valid == 0) {
        return -1;
    }
    *paddr = pte->frameAddr << 12;
    return 0;
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr;
	u_int32_t ehi, elo;
	struct addrspace *as;
	int i, spl;

	spl = splhigh();

	faultaddress &= PAGE_FRAME;
    
	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		splx(spl);
		return EINVAL;
	}

	as = curthread->t_vmspace;
	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

    if (GetPhysicalFrame(&as->pageTable, faultaddress, &paddr) != 0) {
        splx(spl);
		return EFAULT;
    }
    // assert(paddr2 == paddr);
	/* make sure it's page-aligned */
	assert((paddr & PAGE_FRAME)==paddr);

	for (i=0; i<NUM_TLB; i++) {
		TLB_Read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		TLB_Write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	// as->as_vbase1 = 0;
	// as->as_pbase1 = 0;
	// as->as_npages1 = 0;
	// as->as_vbase2 = 0;
	// as->as_pbase2 = 0;
	// as->as_npages2 = 0;
	// as->as_stackpbase = 0;
    bzero(as, sizeof(struct addrspace));

	return as;
}

void ReleasePageTable(PageTableL1* pageTable)
{
    unsigned i, j;
    
    for (i = 0; i < 512; i++) {
        if (pageTable->pageTableL2[i]) {
            PageTableL2* ptl2 = pageTable->pageTableL2[i];
            for (j = 0; j < 1024; j++) {
                if (ptl2->pte[j].valid == 1) {
                    FreeNPages ((ptl2->pte[j].frameAddr) << 12);
                }
            }
            kfree(ptl2);
        }
    }
}

void
as_destroy(struct addrspace *as)
{
    int spl = splhigh();
    ReleasePageTable(&as->pageTable);
	kfree(as);
    splx(spl);
}

void
as_activate(struct addrspace *as)
{
	int i, spl;

	(void)as;

	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

int AddOneMapping(PageTableL1 *pageTable, vaddr_t vaddr, paddr_t paddr)
{
    assert((paddr & 0xfffff000) == paddr);
    assert((vaddr & 0xfffff000) == vaddr);
    
    vaddr = (unsigned)vaddr >> 12;
    u_int32_t pageTableL1Index = vaddr >> 10;
    u_int32_t pageTableL2Index = vaddr & 0x3ff;
    
    PageTableL2* pageTableL2 = pageTable->pageTableL2[pageTableL1Index];
    if (pageTableL2 == NULL) {
        pageTableL2 = kmalloc(sizeof(PageTableL2));
        if (pageTableL2 == NULL) {
            return ENOMEM;
        }
        bzero(pageTableL2, sizeof(PageTableL2));
        pageTable->pageTableL2[pageTableL1Index] = pageTableL2;
    }
    PageTableEntry *pte = &(pageTableL2->pte[pageTableL2Index]);
    assert(pte->valid == 0);
    
    pte->frameAddr = paddr >> 12;
    pte->valid = 1;
    pte->writable = 1;
    return 0;
}

int AddNPagesOnVaddr(PageTableL1 *pageTable, vaddr_t vaddr, size_t npages)
{
    paddr_t paddr;
    unsigned i;
    for (i = 0; i < npages; i++) {
        paddr = getppages(1);
        if (paddr == 0)
            return ENOMEM;
        if (AddOneMapping(pageTable, vaddr + i * PAGE_SIZE, paddr)) {
            return ENOMEM;
        }
    }
    return 0;
}

int
as_prepare_load(struct addrspace *as)
{
    assert(as->as_vbase1 != 0);
    assert(as->as_vbase2 != 0);
    
    if (AddNPagesOnVaddr(&as->pageTable, as->as_vbase1, as->as_npages1)) {
        return ENOMEM;
    }
    
    if (AddNPagesOnVaddr(&as->pageTable, as->as_vbase2, as->as_npages2)) {
        return ENOMEM;
    }
    
    if (AddNPagesOnVaddr(&as->pageTable, 
        USERTOP - DUMBVM_STACKPAGES * PAGE_SIZE, DUMBVM_STACKPAGES)) {
        return ENOMEM;
    }
    
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	// assert(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

void CopyOnePage(PageTableL1 *dest, PageTableL1 *src, vaddr_t vaddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    
    paddr_t paddrdest, paddrsrc;
    if (GetPhysicalFrame(dest, vaddr, &paddrdest)) {
        panic("copy to invalid page!");
    }
    if (GetPhysicalFrame(src, vaddr, &paddrsrc)) {
        panic("copy from invalid page!");
    }
    
    vaddr_t kvaddrdest = PADDR_TO_KVADDR(paddrdest);
    vaddr_t kvaddrsrc = PADDR_TO_KVADDR(paddrsrc);
    memmove((void *)kvaddrdest, (const void *)kvaddrsrc, PAGE_SIZE);
}

void CopyNPages(PageTableL1 *dest, PageTableL1 *src, vaddr_t vaddr, size_t npages)
{
    assert((vaddr & 0xfffff000) == vaddr);
    
    unsigned i;
    for (i = 0; i < npages; i++) {
        CopyOnePage(dest, src, vaddr + i * PAGE_SIZE);
    }
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	// assert(new->as_pbase1 != 0);
	// assert(new->as_pbase2 != 0);
	// assert(new->as_stackpbase != 0);

    CopyNPages(&new->pageTable, &old->pageTable, 
        new->as_vbase1, new->as_npages1);
	// memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		// (const void *)PADDR_TO_KVADDR(old->as_pbase1),
		// old->as_npages1*PAGE_SIZE);

    CopyNPages(&new->pageTable, &old->pageTable, 
        new->as_vbase2, new->as_npages2);
	// memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		// (const void *)PADDR_TO_KVADDR(old->as_pbase2),
		// old->as_npages2*PAGE_SIZE);
    CopyNPages(&new->pageTable, &old->pageTable, 
        USERTOP - DUMBVM_STACKPAGES * PAGE_SIZE, DUMBVM_STACKPAGES);
	// memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		// (const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		// DUMBVM_STACKPAGES*PAGE_SIZE);
	
	*ret = new;
	return 0;
}


