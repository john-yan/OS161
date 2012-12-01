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
#include <uio.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/unistd.h>
#include <synch.h>

static void CopyOnePage(PageTableL1 *dest, PageTableL1 *src, vaddr_t vaddr);
static void CopyNPages(PageTableL1 *dest, PageTableL1 *src, vaddr_t vaddr, size_t npages);
static int AddOneMapping(struct addrspace *as, vaddr_t vaddr, paddr_t *paddr);
static int SetPageValid(PageTableL1 *pageTable, vaddr_t vaddr, unsigned isValid);
static int SetPageWritable(PageTableL1 *pageTable, vaddr_t vaddr, unsigned writable);
static int IsPageValid(PageTableL1 *pageTable, vaddr_t vaddr);
static int AddNPagesOnVaddr(struct addrspace *as, vaddr_t vaddr, size_t npages);
static void ReleasePageTable(PageTableL1* pageTable);
static int GetPhysicalFrame(PageTableL1 *ptl1, vaddr_t vaddr, paddr_t *paddr, unsigned *permission);
static int UpdateTLB(vaddr_t vaddr, paddr_t paddr, unsigned permission);
static int
LoadPage(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr);
static int IsOnStackRegion(size_t stacksize, vaddr_t vaddr);
static int IncreaseStack(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr);

static struct lock vmlock;
static int noProgress = 0;
static struct vnode *disk = NULL;
static struct cv needToEvicPage;
static struct cv needMorePages;
static int isInit = 0;
// static struct semaphore pagesToEvic;

static void swapper(void *o, unsigned long l)
{
    (void)o;
    (void)l;
    paddr_t userPage;
    int spl;
    int pageInDisk = 0;
    int result = 0;
    int nFreePages = 0;
    int i;
    
    do {
        lock_acquire(&vmlock);
        
        spl = splhigh();
        nFreePages = CoreMapReport();
        splx(spl);
        
        while (nFreePages > 10) {
            cv_wait(&needToEvicPage, &vmlock);
            spl = splhigh();
            nFreePages = CoreMapReport();
            splx(spl);
        }
        
        kprintf("put some pages to disk.\n");
        // lock the pagetable and coremap entry
        lock_release(&vmlock);
        // clocksleep(1); // put some pages to disk
        for (i = 0; i < 10000; i++)
            thread_yield(); // some delay
        cv_broadcast(&needMorePages, &vmlock);
        // spl = splhigh();
        // userPage = 0; // get a user page;
        // lock the page
        // splx(spl);
        
        // write the page to disk
        
        // spl = splhigh();
        // unlock the page
        
        // splx(spl);
    } while (1);
}

void
vm_bootstrap(void)
{
    int result;
    struct uio ku;
    lock_init(&vmlock);
    cv_init(&needToEvicPage);
    // sem_init(&pagesToEvic, 0);
    cv_init(&needMorePages);
    // char *p = alloc_kpages(1);
    // bzero(p, PAGE_SIZE);
    // strcpy(p, "hello world.");
    
    // result = vfs_open("lhd0raw:", O_RDWR, &disk);
    // if (result) {
        // panic("Can't Open swap device.");
    // }
    
    // mk_kuio(&ku, p , PAGE_SIZE, 0, UIO_WRITE);
    // result = VOP_WRITE(disk, &ku);
    // if (result) {
        // panic(strerror(result));
    // }
    
    // bzero(p, PAGE_SIZE);
    
    // mk_kuio(&ku, p , PAGE_SIZE, 0, UIO_READ);
    // result = VOP_READ(disk, &ku);
    // if (result) {
        // panic(strerror(result));
    // }
    
    // if (strcmp(p, "hello world.")) {
        // panic("not match.");
    // }
    
    result = thread_fork("swapper", NULL, 0, swapper, NULL);
    if (result) {
        panic("Can't create swapper thread.");
    }
    
    isInit = 0;
}

static
paddr_t
getppages(struct addrspace *as, PageTableEntry *pte)
{
	int spl;
	paddr_t addr = 0;
    const int isKernelPage = 0;
    const int nPageToAllocate = 1;
    
    while (addr == 0) {
        lock_acquire(&vmlock);
        spl = splhigh();
        
        addr = GetNFreePage(1);
        // kprintf("getpages.\n");
        if (addr) {
            AllocateNPages(addr, isKernelPage, nPageToAllocate);
            CoreMapSetAddrSpace(addr, as);
            CoreMapSetPTE(addr, pte);
        }
        // CoreMapReport();
        splx(spl);
        if (addr == 0) {
            cv_signal(&needToEvicPage, &vmlock);
            cv_wait(&needMorePages, &vmlock);
            kprintf("I hope to have more pages.\n");
        }
        lock_release(&vmlock);
    }
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	int spl;
	paddr_t addr;
    const int isKernelPage = 1;
	spl = splhigh();
    
	addr = GetNFreePage(npages);
    // kprintf("getpages.\n");
	if (addr) 
        AllocateNPages(addr, isKernelPage, npages);
    // CoreMapReport();
	splx(spl);
    
    if (isInit) {
        lock_acquire(&vmlock);
        cv_signal(&needToEvicPage, &vmlock);
        lock_release(&vmlock);
    }
	return addr ? PADDR_TO_KVADDR(addr) : 0;
    
}

void 
free_kpages(vaddr_t addr)
{
    int spl = splhigh();
    FreeNPages(KVADDR_TO_PADDR(addr));
    // CoreMapReport();
	splx(spl);
    if (isInit) {
        lock_acquire(&vmlock);
        cv_signal(&needMorePages, &vmlock);
        lock_release(&vmlock);
    }
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr = 0;
	struct addrspace *as;
	int i, spl, result;
    unsigned permission = TLBLO_DIRTY | TLBLO_VALID;
    
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
        splx(spl);
		return EFAULT;
	}

    result = GetPhysicalFrame(&as->pageTable, faultaddress, &paddr, NULL);
    if (result) {
        // decide which region
        if (IsOnStackRegion(as->stacksize, faultaddress)) {
            result = IncreaseStack(as, faultaddress, &paddr);
        } else {
            for (i = 0; i < 2; i++) {
                vaddr_t start = as->region[i].regionBase << 12;
                vaddr_t end = as->region[i].nPages * PAGE_SIZE + start;
                if (faultaddress >= start && faultaddress < end)
                    break;
            }
            if (i < 2) {
                result = LoadPage(as, faultaddress, &paddr);
            }
        }
    }
    
    if (result) {
        splx(spl);
		return EFAULT;
    }

    result = UpdateTLB(faultaddress, paddr, permission);
    if (result) {
        splx(spl);
        return result;
    }
	splx(spl);
	return 0;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		goto fail1;
	}

    bzero(as, sizeof(struct addrspace));
    
    as->lk = lock_create("as lock");
    if (as->lk == NULL) {
        goto fail2;
    }

	return as;
    
fail2:
    kfree(as);
fail1:
    return NULL;
}

void
as_destroy(struct addrspace *as)
{
    int spl = splhigh();
    ReleasePageTable(&as->pageTable);
    if (as->v) VOP_DECREF(as->v);
    if (as->lk) lock_destroy(as->lk);
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
as_define_eh(struct addrspace *as, struct vnode *v, Elf_Ehdr* eh)
{
    int result, i, phi = 0;
	struct uio ku;
    Elf_Phdr elf_ph;
    
    /*
	 * Check to make sure it's a 32-bit ELF-version-1 executable
	 * for our processor type. If it's not, we can't run it.
	 */

	if (eh->e_ident[EI_MAG0] != ELFMAG0 ||
	    eh->e_ident[EI_MAG1] != ELFMAG1 ||
	    eh->e_ident[EI_MAG2] != ELFMAG2 ||
	    eh->e_ident[EI_MAG3] != ELFMAG3 ||
	    eh->e_ident[EI_CLASS] != ELFCLASS32 ||
	    eh->e_ident[EI_DATA] != ELFDATA2MSB ||
	    eh->e_ident[EI_VERSION] != EV_CURRENT ||
	    eh->e_version != EV_CURRENT ||
	    eh->e_type!=ET_EXEC ||
	    eh->e_machine!=EM_MACHINE) {
		return ENOEXEC;
	}
    
    assert(as->v == NULL);
    as->v = v;
    VOP_INCREF(v);
    for (i=0; i<eh->e_phnum; i++) {
        
		off_t offset = eh->e_phoff + i*eh->e_phentsize;
		mk_kuio(&ku, &elf_ph, sizeof(Elf_Phdr), offset, UIO_READ);

		result = VOP_READ(v, &ku);
		if (result) {
			return result;
		}

		if (ku.uio_resid != 0) {
			/* short read; problem with executable? */
			kprintf("ELF: short read on phdr - file truncated?\n");
			return ENOEXEC;
		}

		switch (elf_ph.p_type) {
		    case PT_NULL: /* skip */ continue;
		    case PT_PHDR: /* skip */ continue;
		    case PT_MIPS_REGINFO: /* skip */ continue;
		    case PT_LOAD: break;
		    default:
			kprintf("loadelf: unknown segment type %d\n", 
				elf_ph.p_type);
			return ENOEXEC;
		}
        if (phi >= 2) {
            kprintf("dumbvm: Warning: too many regions\n");
            return EUNIMP;
        }
        
        as->elf_ph[phi] = elf_ph;
        size_t sz = (elf_ph.p_memsz 
                        + (elf_ph.p_vaddr & (~(vaddr_t)PAGE_FRAME)) 
                        + PAGE_SIZE - 1)
                    & PAGE_FRAME;

        as->region[phi].nPages = sz >> 12;
        as->region[phi].regionBase = (elf_ph.p_vaddr & PAGE_FRAME) >> 12;
        
        phi++;
	}
    
    return 0;
}

int
as_prepare_load(struct addrspace *as)
{
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
    as->stacksize = 0;
	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;
    int i;
    
	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

    for (i = 0; i < 2; i++) {
        new->region[i] = old->region[i];
        new->elf_ph[i] = old->elf_ph[i];
    }
    new->stacksize = old->stacksize;
    new->v = old->v;
    VOP_INCREF(new->v);
    
	if (AddNPagesOnVaddr(new, 
            USERTOP - old->stacksize * PAGE_SIZE,
            old->stacksize)) {
		as_destroy(new);
		return ENOMEM;
	}

    CopyNPages(&new->pageTable, &old->pageTable, 
        USERTOP - old->stacksize * PAGE_SIZE, old->stacksize);
	
	*ret = new;
	return 0;
}

int as_hold(struct addrspace *as)
{
    assert(as != NULL);
    assert(as->lk != NULL);
    
    lock_acquire(as->lk);
    return 0;
}

int as_release(struct addrspace *as)
{
    assert(as != NULL);
    assert(as->lk != NULL);
    
    lock_release(as->lk);
    return 0;
}

static void CopyOnePage(PageTableL1 *dest, PageTableL1 *src, vaddr_t vaddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    
    paddr_t paddrdest, paddrsrc;
    unsigned permission;
    
    if (GetPhysicalFrame(dest, vaddr, &paddrdest, NULL)) {
        panic("copy to invalid page!");
    }
    if (GetPhysicalFrame(src, vaddr, &paddrsrc, &permission)) {
        panic("copy from invalid page!");
    }
    
    vaddr_t kvaddrdest = PADDR_TO_KVADDR(paddrdest);
    vaddr_t kvaddrsrc = PADDR_TO_KVADDR(paddrsrc);
    memmove((void *)kvaddrdest, (const void *)kvaddrsrc, PAGE_SIZE);
}

static void CopyNPages(PageTableL1 *dest, PageTableL1 *src, vaddr_t vaddr, size_t npages)
{
    assert((vaddr & 0xfffff000) == vaddr);
    
    unsigned i;
    for (i = 0; i < npages; i++) {
        CopyOnePage(dest, src, vaddr + i * PAGE_SIZE);
    }
}

static int AddOneMapping(struct addrspace *as, vaddr_t vaddr, paddr_t *_paddr)
{
    PageTableL1 *pageTable = &as->pageTable;
    assert(_paddr != NULL);
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
    
    paddr_t paddr = getppages(as, pte);
    if (paddr == 0) {
        return ENOMEM;
    }
    assert((paddr & 0xfffff000) == paddr);
    pte->frameAddr = paddr >> 12;
    *_paddr = paddr;
    return 0;
}

static int SetPageWritable(PageTableL1 *pageTable, vaddr_t vaddr, unsigned writable)
{
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
    pte->writable = writable;
    return 0;
}

static int SetPageValid(PageTableL1 *pageTable, vaddr_t vaddr, unsigned isValid)
{
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
    pte->valid = isValid;
    return 0;
}

static int IsPageWritable(PageTableL1 *pageTable, vaddr_t vaddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    
    vaddr = (unsigned)vaddr >> 12;
    u_int32_t pageTableL1Index = vaddr >> 10;
    u_int32_t pageTableL2Index = vaddr & 0x3ff;
    
    PageTableL2* pageTableL2 = pageTable->pageTableL2[pageTableL1Index];
    if (pageTableL2 == NULL) {
        pageTableL2 = kmalloc(sizeof(PageTableL2));
        if (pageTableL2 == NULL) {
            return 0;
        }
        bzero(pageTableL2, sizeof(PageTableL2));
        pageTable->pageTableL2[pageTableL1Index] = pageTableL2;
    }
    PageTableEntry *pte = &(pageTableL2->pte[pageTableL2Index]);
    return pte->writable;
}

static int IsPageValid(PageTableL1 *pageTable, vaddr_t vaddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    
    vaddr = (unsigned)vaddr >> 12;
    u_int32_t pageTableL1Index = vaddr >> 10;
    u_int32_t pageTableL2Index = vaddr & 0x3ff;
    
    PageTableL2* pageTableL2 = pageTable->pageTableL2[pageTableL1Index];
    if (pageTableL2 == NULL) {
        return 0;
    }
    PageTableEntry *pte = &(pageTableL2->pte[pageTableL2Index]);
    return pte->valid;
}

static int AddNPagesOnVaddr(struct addrspace *as, vaddr_t vaddr, size_t npages)
{
    PageTableL1 *pageTable = &as->pageTable;
    paddr_t paddr;
    unsigned i;
    for (i = 0; i < npages; i++) {
        if (AddOneMapping(as, vaddr + i * PAGE_SIZE, &paddr)) {
            return ENOMEM;
        }
        SetPageValid(&as->pageTable, vaddr, 1);
        SetPageWritable(pageTable, vaddr + i * PAGE_SIZE, 1);
    }
    return 0;
}

static void ReleasePageTable(PageTableL1* pageTable)
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

static int GetPhysicalFrame(PageTableL1 *ptl1, vaddr_t vaddr, paddr_t *paddr, unsigned *permission)
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
    if (permission) *permission = *((int*)pte) & 0x00000fff;
    return 0;
}

static int UpdateTLB(vaddr_t vaddr, paddr_t paddr, unsigned permission)
{
    unsigned i;
    u_int32_t ehi, elo;
    int spl = splhigh();
    assert((paddr & PAGE_FRAME)==paddr);

	for (i = 0; i < NUM_TLB; i++) {
		TLB_Read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = vaddr;
		elo = paddr | permission;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", vaddr, paddr);
		TLB_Write(ehi, elo, i);
		splx(spl);
		return 0;
	}
    
    as_activate(curthread->t_vmspace); // invalid all entry
    ehi = vaddr;
    elo = paddr | permission;
    TLB_Write(ehi, elo, i);
	splx(spl);
	return EFAULT;
}

static int
LoadPage(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    
    paddr_t paddr;
    off_t p_offset;
    vaddr_t p_vaddr;
    size_t filesize;
    size_t memsize;
    unsigned i;
    int spl, result;
    struct uio ku;
    struct vnode *v = as->v;
    assert(v != NULL);
        
    result = AddOneMapping(as, vaddr, &paddr);
    if (result) {
        goto fail1;
    } 
    result = SetPageValid(&as->pageTable, vaddr, 1);
    assert(result == 0);
    
    bzero((void*)KVADDR_TO_PADDR(paddr), PAGE_SIZE);
    
    for (i = 0; i < 2; i++) {
        p_vaddr = as->elf_ph[i].p_vaddr;
        p_offset = as->elf_ph[i].p_offset;
        filesize = as->elf_ph[i].p_filesz;
        memsize = as->elf_ph[i].p_memsz;
        if (vaddr >= p_vaddr && vaddr < p_vaddr + memsize) {
            if (filesize > memsize) {
                kprintf("ELF: warning: segment filesize > segment memsize\n");
                filesize = memsize;
            }
            vaddr_t startaddr = (vaddr > p_vaddr ? vaddr : p_vaddr);
            size_t memLen = PAGE_SIZE - ((~(vaddr_t)PAGE_FRAME) & startaddr);
            off_t offset = p_offset + startaddr - p_vaddr;
            
            if (filesize + p_offset > (unsigned)offset)
                filesize = filesize + p_offset - offset;
            size_t tranSize = filesize > memLen ? memLen : filesize;
            assert(tranSize <= PAGE_SIZE);
            
            vaddr_t kstartaddr = KVADDR_TO_PADDR(paddr + startaddr - vaddr);
            mk_kuio(&ku, kstartaddr, tranSize, offset, UIO_READ);
            
            result = VOP_READ(v, &ku);
            if (result) {
                return result;
            }

            if (ku.uio_resid != 0) {
                /* short read; problem with executable? */
                kprintf("ELF: short read on segment - file truncated?\n");
                return ENOEXEC;
            }
            result = SetPageWritable(&as->pageTable, vaddr, (as->elf_ph[i].p_flags & PF_W) != 0);
            assert(result == 0);
        }
    }
    *_paddr = paddr;
    return 0;
    
fail1:
    return ENOMEM;
}

static int IsOnStackRegion(size_t stacksize, vaddr_t vaddr) 
{
    vaddr_t start = USERSTACK - stacksize * PAGE_SIZE;
    vaddr_t end = USERSTACK;
    
    if (vaddr < end && vaddr >= (start - PAGE_SIZE))
        return 1;
    else
        return 0;
}

static int IncreaseStack(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    
    const unsigned writable = 1;
    paddr_t paddr;
    int spl;
    
    spl = splhigh();
    if (AddOneMapping(as, vaddr, &paddr)) {
        goto fail1;
    }
    SetPageValid(&as->pageTable, vaddr, 1);
    SetPageWritable(&as->pageTable, vaddr, writable);
    as->stacksize++;
    
    splx(spl);
    bzero((void*)KVADDR_TO_PADDR(paddr), PAGE_SIZE);
    
    *_paddr = paddr;
    return 0;
    
fail1:
    splx(spl);
    return ENOMEM;
}




