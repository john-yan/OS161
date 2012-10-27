#include <types.h>
#include <thread.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <kern/callno.h>
#include <syscall.h>
#include <addrspace.h>
#include <array.h>
#include <synch.h>
#include <thread.h>

int sys_waitpid(int pid, int* status, int options)
{
    (void)status; 
    (void)options;
    
    int i, totalchildren = array_getnum(curthread->t_miPCB->children);
    struct MiPCB *cmipcb = NULL;
    // find the child with the specific pid
    for (i = 0; i < totalchildren; i ++) {
        cmipcb = array_getguy(curthread->t_miPCB->children, i);
        if (cmipcb->processID == pid) {
            break;
        }
        cmipcb = NULL;
    }
    
    // can't find a child with pid
    if (cmipcb == NULL) {
        return -1;
    }
    
    Down(cmipcb->waitOnExit);
    
    assert(cmipcb->isExit == 1);
    // *status = cmipcb->exitCode;
    MiPCBDestroy(cmipcb);
    array_remove(curthread->t_miPCB->children, i);
    
    return 0;
    
    
}
