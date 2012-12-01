#include <coremap.h>
#include <types.h>
#include <lib.h>
#include <vm.h>
#include <addrspace.h>
#include <machine/spl.h>

static struct {
    u_int32_t  totalNumberOfPages;
    u_int32_t  firstPhysicalPageAddr;
    PageEntry* firstEntry;
    PageEntry* freeList; // unused, intend to keep a list of free entrys
    
} coremap;

#define GETPADDR(pe) ((coremap.firstPhysicalPageAddr) + \
                            ((pe) - coremap.firstEntry) * _4K_)
#define GETPAGEENTRY(paddr) (coremap.firstEntry + \
            (((u_int32_t)(paddr) - coremap.firstPhysicalPageAddr) >> 12))
#define ISPAGEFREE(pe) ((pe)->isAllocated == 0)
#define SETALLOCATED(pe, k) ((pe)->isAllocated = 1; (pe)->isKernelPage = k;)
#define SETFREE(pe) ((pe)->isAllocated = 0)
#define LASTENTRY (coremap.firstEntry + coremap.totalNumberOfPages)
#define FIRSTENTRY (coremap.firstEntry)

void CoreMapBootsTrap()
{
    u_int32_t lo, hi;
    ram_getsize(&lo, &hi);
    
    // determine the address of first page
    coremap.firstPhysicalPageAddr = GET_INIT_PAGE_BOUNDARY(lo);
    
    // determine the address of last page
    u_int32_t lastPage = GET_INIT_PAGE_BOUNDARY(hi);
    
    // determine how many pages in total
    coremap.totalNumberOfPages = (lastPage - coremap.firstPhysicalPageAddr) >> 12;
    
    // determine the coremap size
    u_int32_t mapSize = coremap.totalNumberOfPages * sizeof(PageEntry);
    
    // determine how many pages needs to store the map
    u_int32_t pagesNeeded = mapSize / 4096 + 1;
    
    // allocate the first pagesNeeded pages for the map
    // initialize all entrys
    // initialize the first pagesNeeded pages
    coremap.firstEntry = (PageEntry*)(coremap.firstPhysicalPageAddr + MIPS_KSEG0);
    bzero(coremap.firstEntry, coremap.totalNumberOfPages * sizeof(PageEntry));
    coremap.firstEntry += pagesNeeded;
    coremap.totalNumberOfPages -= pagesNeeded;
    coremap.firstPhysicalPageAddr += (pagesNeeded << 12);
    // coremap.firstEntry->nContinousPages = coremap.totalNumberOfPages;
    
    kprintf("Coremap is initialized with %d free pages.\n", 
        coremap.totalNumberOfPages);
}

static int pages = 0;

u_int32_t GetNFreePage(u_int32_t nPages)
{
    PageEntry* peStart = FIRSTENTRY;
    PageEntry* peEnd;
    assert(curspl > 0);
    while (peStart < LASTENTRY) {
        while (peStart < LASTENTRY && !ISPAGEFREE(peStart)) 
            peStart++;
        peEnd = peStart;
        while (peEnd < LASTENTRY && ISPAGEFREE(peEnd)) 
            peEnd++;
        if ((u_int32_t)(peEnd - peStart) > nPages) {
            return GETPADDR(peStart);
        }
        peStart = peEnd;
    }
    
    return 0;
}

void AllocateNPages(struct addrspace *as, u_int32_t paddr, u_int32_t isKernelPage, u_int32_t nPages)
{
    assert(((u_int32_t)paddr & 0xfffff000) == paddr);
    if (isKernelPage == 0) {
        assert(as != NULL);
    } else {
        assert(as == NULL);
    }
    assert(curspl > 0);
    unsigned int i = 0;
    PageEntry* pe = GETPAGEENTRY(paddr);
    pe->nContinousPages = nPages;
    for (i = 0; i < nPages; i++) {
        assert(pe[i].isAllocated == 0);
        pe[i].isAllocated = 1;
        pe[i].isKernelPage = isKernelPage;
        pe[i].ref = as;
    }
    // kprintf("allocate %d pages(%d).\n",nPages, pages += nPages);
}

void FreeNPages(u_int32_t paddr) 
{
    assert(curspl > 0);
    assert(((u_int32_t)paddr & 0xfffff000) == paddr);
    PageEntry* pe = GETPAGEENTRY(paddr);
    int i = 0, nPages = pe->nContinousPages;
    assert(pe + pe->nContinousPages <= LASTENTRY);
    for (i = 0; i < nPages; i++) {
        assert(pe[i].isAllocated == 1);
        pe[i].isAllocated = 0;
    }
    // kprintf("free %d pages(%d).\n", nPages, pages -= nPages);
}

int CoreMapReport()
{
    unsigned i = 0;
    unsigned numOfFreePages = 0;
    unsigned numOfAllocPages = 0;
    PageEntry* pe = FIRSTENTRY;
    assert(curspl > 0);
    
    for (i = 0; i < coremap.totalNumberOfPages; i++) {
        if (pe[i].isAllocated == 1)
            numOfAllocPages++;
        else
            numOfFreePages++;
    }
    return numOfFreePages;
    // kprintf("number of free pages = %d\n", numOfFreePages);
    // kprintf("number of allocated pages = %d\n", numOfAllocPages);
}

int CoreMapGetPageToSwap(struct addrspace **as, 
            struct _PageTableEntry **pte)
{
    // unsigned i = 0;
    // PageEntry* pe = FIRSTENTRY;
    // paddr_t paddr;
    // assert(curspl > 0);
    // assert(kvaddr != NULL);
    
    // for (i = 0; i < coremap.totalNumberOfPages; i++) {
        // if (pe[i].isAllocated == 1 && pe[i].isKernelPage == 0) {
            // paddr = GETPADDR(&pe[i]);
            // *kvaddr = PADDR_TO_KVADDR(paddr);
            // return 0;
        // }
    // }
    // return -1;
}

int CoreMapSetPageInfo(struct addrspace *as, PageTableEntry * pte, paddr_t paddr)
{
    
}







