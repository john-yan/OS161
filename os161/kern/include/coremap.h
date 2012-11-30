#ifndef _COREMAP_H
#define _COREMAP_H
#include <coremap.h>
#include <types.h>
#include <lib.h>
#include <vm.h>
#define _4K_ (1<<12)
#define GET_INIT_PAGE_BOUNDARY(x) ((x)&0xfffff000)
#define ADDROFPAGEN(n) ((n)<<12)

// typedef struct {
    // u_int32_t vpaddr: 20;
    // u_int32_t pid: 6;
    // u_int32_t unused: 6;
// } TLBHiEntry;

// typedef struct {
    // u_int32_t ppaddr: 20;
    // u_int32_t noCache: 1; // unused
    // u_int32_t dirty: 1;
    // u_int32_t valid: 1;
    // u_int32_t global: 1; // ignore pid bits if set
    // u_int32_t kernelPage: 1; // this is a kernel page if set
    // u_int32_t unused: 7;
// } TLBLoEntry;

// typedef struct _PageEntry{
    // TLBLoEntry tlblo;
    // TLBHiEntry tlbhi;
    // u_int32_t  isAllocated: 1;
    // u_int32_t  unused: 31;
    /* other info */
    // struct _PageEntry* next;
    
// } PageEntry;

void CoreMapBootsTrap();
u_int32_t GetNFreePage(u_int32_t nPages);
void AllocateNPages(u_int32_t paddr, u_int32_t isKernelPage, u_int32_t nPages);
void FreeNPages(u_int32_t paddr);
int CoreMapReport();

#endif










