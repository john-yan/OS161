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
#include <bitmap.h>
#define SECTORS 10240
#define BYTESPERSECTOR 512
#define DISKSPACE (SECTORS * BYTESPERSECTOR)

static int CopyOnePage(struct addrspace *destas, struct addrspace *srcas, vaddr_t vaddr);
static int CopyNPages(struct addrspace *destas, struct addrspace *srcas, vaddr_t vaddr, size_t npages);
static int AddOneMapping(struct addrspace *as, vaddr_t vaddr, paddr_t *paddr);
static int SetPageValid(PageTableL1 *pageTable, vaddr_t vaddr, unsigned isValid);
static int SetPageWritable(PageTableL1 *pageTable, vaddr_t vaddr, unsigned writable);
static int IsPageValid(PageTableL1 *pageTable, vaddr_t vaddr);
static int AddNPagesOnVaddr(struct addrspace *as, vaddr_t vaddr, size_t npages);
static void ReleasePageTable(PageTableL1* pageTable);
static int GetPhysicalFrame(struct addrspace *as, vaddr_t vaddr, paddr_t *paddr, unsigned *permission);
static int UpdateTLB(vaddr_t vaddr, paddr_t paddr, unsigned permission);
static int
LoadPage(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr, unsigned *permission);
static int IsOnStackRegion(size_t stacksize, vaddr_t vaddr);
static int IncreaseStack(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr);
static int AllocateHeap(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr);

static struct lock vmlock;
static int noProgress = 0;
static struct vnode *disk[2] = {NULL, NULL};
static struct cv needToEvicPage;
static struct cv needMorePages;
static int isInit = 0;
// static int nextEmptySpace = 0;
static struct bitmap *diskmap = NULL;
// static struct semaphore pagesToEvic;

static u_int32_t ToDisk(vaddr_t vaddr)
{
    assert(lock_do_i_hold(&vmlock));
    assert((vaddr & 0xfffff000) == vaddr);
    
    struct uio ku;
    int result;
    u_int32_t bitmaploc, diskloc, diskindex, flatloc;
    result = bitmap_alloc(diskmap, &bitmaploc);
    
    if (result) {
        panic("no disk space.");
    }
    
    flatloc = bitmaploc * PAGE_SIZE;
    diskindex = flatloc / DISKSPACE;
    diskloc = flatloc - diskindex * DISKSPACE;
    
    mk_kuio(&ku, (void*)vaddr , PAGE_SIZE, diskloc, UIO_WRITE);
    result = VOP_WRITE(disk[diskindex], &ku);
    if (result) {
        panic(strerror(result));
    }
    return flatloc;
}

static int ToMem(u_int32_t flatloc, vaddr_t vaddr)
{
    assert(lock_do_i_hold(&vmlock));
    assert((vaddr & 0xfffff000) == vaddr);
    assert((flatloc & 0xfffff000) == flatloc);
    
    struct uio ku;
    int result;
    u_int32_t bitmaploc, diskloc, diskindex;
    bitmaploc = flatloc/PAGE_SIZE;
    
    assert (bitmap_isset(diskmap, bitmaploc));
    
    diskindex = flatloc / DISKSPACE;
    diskloc = flatloc - diskindex * DISKSPACE;
    
    mk_kuio(&ku, (void*)vaddr , PAGE_SIZE, diskloc, UIO_READ);
    result = VOP_READ(disk[diskindex], &ku);
    if (result) {
        panic(strerror(result));
    }
    
    bitmap_unmark(diskmap, bitmaploc);
    return result;
}

static int DiskClear(u_int32_t flatloc)
{
    assert(lock_do_i_hold(&vmlock));
    assert((flatloc & 0xfffff000) == flatloc);
    
    u_int32_t bitmaploc;
    bitmaploc = flatloc/PAGE_SIZE;
    
    assert (bitmap_isset(diskmap, bitmaploc));
    
    bitmap_unmark(diskmap, bitmaploc);
    return 0;
}

static void swapper(void *o, unsigned long l)
{
    (void)o;
    (void)l;
    int spl;
    int result = 0;
    int nFreePages = 0;
    struct addrspace* as;
    PageTableEntry *pte;
    paddr_t paddr;
    vaddr_t kvaddr;
    u_int32_t loc;
    
    do {
        lock_acquire(&vmlock);
        
        spl = splhigh();
        nFreePages = CoreMapReport();
        splx(spl);
        
        while (nFreePages > 0) {
            cv_broadcast(&needMorePages, &vmlock);
            cv_wait(&needToEvicPage, &vmlock);
            spl = splhigh();
            nFreePages = CoreMapReport();
            splx(spl);
        }
        
        // kprintf("put some pages to disk.\n");
        spl = splhigh();
        result = CoreMapGetPageToSwap(&as, &pte);
        if (!result) {
            assert(pte->valid == 1);
            if (pte->writable == 0) {
                paddr_t paddr = pte->frameAddr << 12;
                FreeNPages(paddr);
                pte->valid = 0;
            } else {
                paddr = pte->frameAddr << 12;
                kvaddr = PADDR_TO_KVADDR(paddr);
                loc = ToDisk(kvaddr);
                assert((loc & 0xfffff000) == loc);
                FreeNPages(paddr);
                pte->frameAddr = loc >> 12;
                pte->valid = 0;
                pte->swapped = 1;
            }
        }
        splx(spl);
        // lock the pagetable and coremap entry
        cv_broadcast(&needMorePages, &vmlock);
        lock_release(&vmlock);
    } while (1);
}

void
vm_bootstrap(void)
{
    int result;
    struct uio ku;
    lock_init(&vmlock);
    cv_init(&needToEvicPage);
    cv_init(&needMorePages);
    diskmap = bitmap_create(DISKSPACE * 2 / PAGE_SIZE);
    
    if (diskmap == NULL) {
        panic("can't create disk map.");
    }
    
    result = vfs_open("lhd0raw:", O_RDWR, &disk[0]);
    if (result) {
        panic("Can't Open swap device.");
    }
    
    result = vfs_open("lhd1raw:", O_RDWR, &disk[1]);
    if (result) {
        panic("Can't Open swap device.");
    }
    
    result = thread_fork("swapper", NULL, 0, swapper, NULL);
    if (result) {
        panic("Can't create swapper thread.");
    }
    
    isInit = 1;
}

static
paddr_t
getppages(struct addrspace *as, PageTableEntry *pte)
{
	int spl;
	paddr_t addr = 0;
    const int isKernelPage = 0;
    const int nPageToAllocate = 1;
    int pageLeft = 0;
    
    while (addr == 0) {
        // lock_acquire(&vmlock);
        assert(lock_do_i_hold(&vmlock));
        spl = splhigh();
        
        addr = GetNFreePage(1);
        // kprintf("getpages.\n");
        if (addr) {
            AllocateNPages(addr, isKernelPage, nPageToAllocate);
            CoreMapSetAddrSpace(addr, as);
            CoreMapSetPTE(addr, pte);
        } else {
            assert(CoreMapReport() == 0);
        }
        pageLeft = CoreMapReport();
        
        splx(spl);
        if (addr == 0) {
            cv_signal(&needToEvicPage, &vmlock);
            cv_wait(&needMorePages, &vmlock);
            // kprintf("I hope to have more pages.\n");
        } else if (pageLeft < 20) {
            cv_signal(&needToEvicPage, &vmlock);
        }
        // lock_release(&vmlock);
    }
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
    int spl;
	paddr_t addr = 0;
    const int isKernelPage = 1;
    const int nPageToAllocate = npages;
    int pageLeft = 0;
    
    while (addr == 0) {
        int hold;
        if (isInit) {
            hold = lock_do_i_hold(&vmlock);
            if (!hold) lock_acquire(&vmlock);
            assert(lock_do_i_hold(&vmlock));
        }
        
        spl = splhigh();
        
        addr = GetNFreePage(nPageToAllocate);
        // kprintf("getpages.\n");
        if (addr) {
            AllocateNPages(addr, isKernelPage, nPageToAllocate);
        } else {
            assert(CoreMapReport() == 0);
        }
        pageLeft = CoreMapReport();
        
        splx(spl);
        if (addr == 0) {
            cv_signal(&needToEvicPage, &vmlock);
            cv_wait(&needMorePages, &vmlock);
            // kprintf("I hope to have more pages.\n");
        } else if (pageLeft < 20) {
            cv_signal(&needToEvicPage, &vmlock);
        }
        if (isInit && !hold) lock_release(&vmlock);
    }
	// int spl;
	// paddr_t addr;
    // const int isKernelPage = 1;
	// spl = splhigh();
    
	// addr = GetNFreePage(npages);
	// if (addr) 
        // AllocateNPages(addr, isKernelPage, npages);
	// splx(spl);
    
    // if (isInit) {
        // int hold = lock_do_i_hold(&vmlock);
        // if (!hold) lock_acquire(&vmlock);
        // cv_signal(&needToEvicPage, &vmlock);
        // if (!hold) lock_release(&vmlock);
    // }
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
        int hold = lock_do_i_hold(&vmlock);
        if (!hold) lock_acquire(&vmlock);
        cv_signal(&needMorePages, &vmlock);
        if (!hold) lock_release(&vmlock);
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

    assert(!lock_do_i_hold(&vmlock));
    lock_acquire(&vmlock);
    result = GetPhysicalFrame(as, faultaddress, &paddr, &permission);
    if (result) {
        // decide which region
        if (IsOnStackRegion(as->stacksize, faultaddress)) {
            result = IncreaseStack(as, faultaddress, &paddr);
            permission = TLBLO_DIRTY | TLBLO_VALID;
        } else if (faultaddress >= as->heapstart 
            && faultaddress < as->heapstart + as->heapsize) {
            result = AllocateHeap(as, faultaddress, &paddr);
            permission = TLBLO_DIRTY | TLBLO_VALID;
        } else {
            for (i = 0; i < 2; i++) {
                vaddr_t start = as->region[i].regionBase << 12;
                vaddr_t end = as->region[i].nPages * PAGE_SIZE + start;
                if (faultaddress >= start && faultaddress < end)
                    break;
            }
            if (i < 2) {
                result = LoadPage(as, faultaddress, &paddr, &permission);
            }
        }
    }
    
    if (result) {
        lock_release(&vmlock);
        splx(spl);
		return EFAULT;
    }

    result = UpdateTLB(faultaddress, paddr, permission);
    if (result) {
        lock_release(&vmlock);
        splx(spl);
        return result;
    }
    lock_release(&vmlock);
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
    lock_acquire(&vmlock);
    ReleasePageTable(&as->pageTable);
    if (as->v) VOP_DECREF(as->v);
    if (as->lk) lock_destroy(as->lk);
	kfree(as);
    lock_release(&vmlock);
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
    u_int32_t heapstart = 0;
    
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
        
        if (heapstart < (elf_ph.p_vaddr & PAGE_FRAME) + sz) {
            heapstart = (elf_ph.p_vaddr & PAGE_FRAME) + sz;
        }
        
        phi++;
	}
    
    assert((heapstart & PAGE_FRAME) == heapstart);
    as->heapstart = heapstart;
    as->heapsize = 0;
    
    return 0;
}

int as_heap(struct addrspace *as, int bytes)
{
    assert(as != NULL);
    assert(bytes >= 0);
    
    
    u_int32_t oldheap = as->heapsize + as->heapstart;
    u_int32_t newsize = as->heapsize + (unsigned)bytes;
    
    if (newsize > 5242880) {
        return -1;
    }
    
    as->heapsize = newsize;
    return oldheap;
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

static int times = 0;

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
    times++;
    
	struct addrspace *new;
    int i, result;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}
    
    lock_acquire(&vmlock);
    for (i = 0; i < 2; i++) {
        new->region[i] = old->region[i];
        new->elf_ph[i] = old->elf_ph[i];
    }
    new->stacksize = old->stacksize;
    new->v = old->v;
    VOP_INCREF(old->v);

    result = CopyNPages(new, old, 
        USERTOP - old->stacksize * PAGE_SIZE, old->stacksize);
	
    if (result) {
        lock_release(&vmlock);
        as_destroy(new);
        return result;
    }
    lock_release(&vmlock);
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

static int CopyOnePage(struct addrspace *destas, struct addrspace *srcas, vaddr_t vaddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    assert(lock_do_i_hold(&vmlock));
    
    paddr_t paddrdest, paddrsrc;
    unsigned permission;
    int result = 0;
    
    char *ktemp = alloc_kpages(1);
    if (ktemp == 0) {
        return ENOMEM;
    }
    
    if (GetPhysicalFrame(srcas, vaddr, &paddrsrc, &permission)) {
        panic("copy from invalid page!");
    }
    
    vaddr_t kvaddrsrc = PADDR_TO_KVADDR(paddrsrc);
    memmove((void *)ktemp, (const void *)kvaddrsrc, PAGE_SIZE);
    
    if (GetPhysicalFrame(destas, vaddr, &paddrdest, NULL)) {
        result = AddOneMapping(destas, vaddr, &paddrdest);
        if (result) {
            free_kpages((vaddr_t)ktemp);
            return result;
        }
        SetPageValid(&destas->pageTable, vaddr, 1);
        SetPageWritable(&destas->pageTable, vaddr, (permission & TLBLO_DIRTY) != 0);
    }
    
    vaddr_t kvaddrdest = PADDR_TO_KVADDR(paddrdest);
    memmove((void *)kvaddrdest, (const void *)ktemp, PAGE_SIZE);
    free_kpages((vaddr_t)ktemp);
    return 0;
}

static int CopyNPages(struct addrspace *destas, struct addrspace *srcas, vaddr_t vaddr, size_t npages)
{
    assert((vaddr & 0xfffff000) == vaddr);
    assert(lock_do_i_hold(&vmlock));
    int result;
    
    unsigned i;
    for (i = 0; i < npages; i++) {
        result = CopyOnePage(destas, srcas, vaddr + i * PAGE_SIZE);
        if (result)
            return result;
    }
    return 0;
}

static int AddOneMapping(struct addrspace *as, vaddr_t vaddr, paddr_t *_paddr)
{
    PageTableL1 *pageTable = &as->pageTable;
    assert(_paddr != NULL);
    assert((vaddr & 0xfffff000) == vaddr);
    assert(lock_do_i_hold(&vmlock));
    
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
    assert(lock_do_i_hold(&vmlock));
    
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
    assert(lock_do_i_hold(&vmlock));
    
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
    assert(lock_do_i_hold(&vmlock));
    
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
    assert(lock_do_i_hold(&vmlock));
    
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
    
    assert(lock_do_i_hold(&vmlock));
    
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
    assert(lock_do_i_hold(&vmlock));
    
    for (i = 0; i < 512; i++) {
        if (pageTable->pageTableL2[i]) {
            PageTableL2* ptl2 = pageTable->pageTableL2[i];
            for (j = 0; j < 1024; j++) {
                if (ptl2->pte[j].valid == 1) {
                    FreeNPages ((ptl2->pte[j].frameAddr) << 12);
                    ptl2->pte[j].valid = 0;
                } else if (ptl2->pte[j].swapped == 1) {
                    u_int32_t loc = (ptl2->pte[j].frameAddr) << 12;
                    DiskClear(loc);
                    ptl2->pte[j].swapped = 0;
                    
                }
            }
            kfree(ptl2);
        }
    }
}

static int GetPhysicalFrame(struct addrspace *as, vaddr_t vaddr, paddr_t *paddr, unsigned *permission)
{
    assert((vaddr & 0xfffff000) == vaddr);
    assert(paddr != NULL);
    assert(as != NULL);
    assert(lock_do_i_hold(&vmlock));
    
    u_int32_t pageTableL1Index = (unsigned)vaddr >> 22;
    u_int32_t pageTableL2Index = ((unsigned)vaddr >> 12) & 0x3ff;
    int result;
    
    PageTableL1* ptl1 = &as->pageTable;
    PageTableL2* pageTableL2 = ptl1->pageTableL2[pageTableL1Index];
    if (pageTableL2 == NULL) {
        return -1;
    }
    PageTableEntry *pte = &(pageTableL2->pte[pageTableL2Index]);
    if (pte->valid == 0) {
        if (pte->swapped == 1) {
            unsigned loc = pte->frameAddr << 12;
            result = AddOneMapping(as, vaddr, paddr);
            if (result) {
                pte->frameAddr = loc >> 12;
                return result;
            }
            vaddr_t kvaddr = PADDR_TO_KVADDR(*paddr);
            ToMem(loc, kvaddr);
            pte->swapped = 0;
            pte->valid = 1;
        } else {
            return -1;
        }
    }
    *paddr = pte->frameAddr << 12;
    if (permission) {
        *permission = TLBLO_VALID;
        if (pte->writable) 
            *permission |= TLBLO_DIRTY;
    }
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
	return 0;
}

static int
LoadPage(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr, unsigned *permission)
{
    assert((vaddr & 0xfffff000) == vaddr);
    assert(lock_do_i_hold(&vmlock));
    assert(as != NULL);
    assert(as->v != NULL);
    
    paddr_t paddr;
    off_t p_offset;
    vaddr_t p_vaddr;
    size_t filesize;
    size_t memsize;
    unsigned i;
    int spl, result;
    struct uio ku;
    struct vnode *v = as->v;
    
    result = AddOneMapping(as, vaddr, &paddr);
    if (result) {
        goto fail1;
    } 
    result = SetPageValid(&as->pageTable, vaddr, 1);
    assert(result == 0);
    
    bzero((void*)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
    
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
            
            vaddr_t kstartaddr = PADDR_TO_KVADDR(paddr + startaddr - vaddr);
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
            if (permission) {
                *permission = TLBLO_VALID;
                if (as->elf_ph[i].p_flags & PF_W) 
                    *permission |= TLBLO_DIRTY;
            }
        }
    }
    *_paddr = paddr;
    return 0;
    
fail1:
    return ENOMEM;
}

static int IsOnStackRegion(size_t stacksize, vaddr_t vaddr) 
{
    assert(lock_do_i_hold(&vmlock));
    
    vaddr_t start = USERSTACK - stacksize * PAGE_SIZE;
    vaddr_t end = USERSTACK;
    
    if (vaddr < end && vaddr >= start) {
        return 1;
    }
    
    if (end - start == 0x500000) {
        return 0;
    }
    
    if (vaddr < end && vaddr >= (start - PAGE_SIZE)){
        return 1;
    }
        
    return 0;
}

static int IncreaseStack(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    assert(lock_do_i_hold(&vmlock));
    
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
    bzero((void*)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
    
    *_paddr = paddr;
    return 0;
    
fail1:
    splx(spl);
    return ENOMEM;
}

static int AllocateHeap(struct addrspace* as, vaddr_t vaddr, paddr_t *_paddr)
{
    assert((vaddr & 0xfffff000) == vaddr);
    assert(lock_do_i_hold(&vmlock));
    const unsigned writable = 1;
    paddr_t paddr;
    int spl;
    
    spl = splhigh();
    if (AddOneMapping(as, vaddr, &paddr)) {
        goto fail1;
    }
    SetPageValid(&as->pageTable, vaddr, 1);
    SetPageWritable(&as->pageTable, vaddr, writable);
    
    splx(spl);
    bzero((void*) PADDR_TO_KVADDR(paddr), PAGE_SIZE);
    
    *_paddr = paddr;
    return 0;
    
fail1:
    splx(spl);
    return ENOMEM;
    
}














