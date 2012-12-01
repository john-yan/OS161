#ifndef _COREMAP_H
#define _COREMAP_H
#include <types.h>
#include <lib.h>
#include <vm.h>
#define _4K_ (1<<12)
#define GET_INIT_PAGE_BOUNDARY(x) ((x)&0xfffff000)
#define ADDROFPAGEN(n) ((n)<<12)

struct _PageTableEntry;
struct addrspace;

typedef struct _PageEntry{
    u_int32_t  isAllocated: 1;
    u_int32_t  isKernelPage: 1;
    u_int32_t  nContinousPages: 20;
    u_int32_t  isLock: 1;
    u_int32_t  unused: 9;
    // /* other info */
    struct addrspace* ref;
    struct _PageTableEntry *pte;
    
} PageEntry;

void CoreMapBootsTrap();
u_int32_t GetNFreePage(u_int32_t nPages);
void AllocateNPages(struct addrspace* as, u_int32_t paddr, u_int32_t isKernelPage, u_int32_t nPages);
void CoreMapSetAddrSpace(u_int32_t paddr, struct addrspace *as);
void CoreMapSetPTE(u_int32_t paddr, struct _PageTableEntry *pte);

void FreeNPages(u_int32_t paddr);
int CoreMapReport();
int CoreMapGetPageToSwap(struct addrspace **as, 
            struct _PageTableEntry **pte);
int CoreMapSetPageInfo(struct addrspace *as, 
            struct _PageTableEntry * pte, paddr_t paddr);
#endif










