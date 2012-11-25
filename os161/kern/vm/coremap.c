#include <coremap.h>
#include <types.h>
#include <lib.h>
#include <vm.h>

typedef struct {
    u_int32_t vpaddr: 20;
    u_int32_t pid: 6;
    u_int32_t unused: 6;
} TLBHiEntry;

typedef struct {
    u_int32_t ppaddr: 20;
    u_int32_t noCache: 1; // unused
    u_int32_t dirty: 1;
    u_int32_t valid: 1;
    u_int32_t global: 1; // ignore pid bits if set
    u_int32_t kernelPage: 1; // this is a kernel page if set
    u_int32_t unused: 7;
} TLBLoEntry;

typedef struct _PageEntry{
    TLBLoEntry tlblo;
    TLBHiEntry tlbhi;
    u_int32_t  isAllocated: 1;
    u_int32_t  isKernelPage: 1;
    u_int32_t  nContinousPages: 20;
    u_int32_t  unused: 10;
    // /* other info */
    struct _PageEntry* next;
    
} PageEntry;

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

void AllocateNPages(u_int32_t paddr, u_int32_t isKernelPage, u_int32_t nPages)
{
    assert(((u_int32_t)paddr & 0xfffff000) == paddr);
    
    unsigned int i = 0;
    PageEntry* pe = GETPAGEENTRY(paddr);
    // assert(pe->nContinousPages >= nPages);
    // if (pe + nPages < LASTENTRY)
        // pe[nPages].nContinousPages = pe->nContinousPages - nPages;
    pe->nContinousPages = nPages;
    for (i = 0; i < nPages; i++) {
        assert(pe[i].isAllocated == 0);
        pe[i].isAllocated = 1;
        pe[i].isKernelPage = isKernelPage;
    }
    // kprintf("allocate %d pages(%d).\n",nPages, pages += nPages);
}

void FreeNPages(u_int32_t paddr) 
{
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

void CoreMapReport()
{
    unsigned i = 0;
    unsigned numOfFreePages = 0;
    unsigned numOfAllocPages = 0;
    PageEntry* pe = FIRSTENTRY;
    for (i = 0; i < coremap.totalNumberOfPages; i++) {
        if (pe[i].isAllocated == 1)
            numOfAllocPages++;
        else
            numOfFreePages++;
    }
    kprintf("number of free pages = %d\n", numOfFreePages);
    kprintf("number of allocated pages = %d\n", numOfAllocPages);
}











