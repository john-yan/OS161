#ifndef _COREMAP_H
#define _COREMAP_H
#include <coremap.h>
#include <types.h>
#include <lib.h>
#include <vm.h>
#define _4K_ (1<<12)
#define GET_INIT_PAGE_BOUNDARY(x) ((x)&0xfffff000)
#define ADDROFPAGEN(n) ((n)<<12)

typedef struct _PageEntry{
    u_int32_t  isAllocated: 1;
    u_int32_t  isKernelPage: 1;
    u_int32_t  nContinousPages: 20;
    u_int32_t  unused: 10;
    // /* other info */
    struct addrspace* ref;
    // PageTableEntry *pte;
    
} PageEntry;

void CoreMapBootsTrap();
u_int32_t GetNFreePage(u_int32_t nPages);
void AllocateNPages(struct addrspace* as, u_int32_t paddr, u_int32_t isKernelPage, u_int32_t nPages);
void FreeNPages(u_int32_t paddr);
int CoreMapReport();
int CoreMapGetPageToSwap(vaddr_t *kvaddr);
// int CoreMapSetPageInfo(struct addrspace *as, PageTableEntry * pte, paddr_t paddr);

#endif










