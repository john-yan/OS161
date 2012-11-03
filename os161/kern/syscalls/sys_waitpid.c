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
#include <uio.h>

int sys_waitpid(int pid, int* status, int options)
{
    (void)status; 
    (void)options;
    struct uio u, u1;
    
    if (options != 0) {
        return -1;
    }
    
    int spl = splhigh();
    
    u.uio_iovec.iov_ubase = (userptr_t)status;
	u.uio_iovec.iov_len = 4;   // length of the memory space
	u.uio_resid = 4;          // amount to actually read
	u.uio_offset = 0;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curthread->t_vmspace;
    u1 = u;
    if (uiomove(&status, 4, &u1) != 0){
        splx(spl);
        return -1;
    }
    
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
        splx(spl);
        return -1;
    }
    
    Down(cmipcb->waitOnExit);
    
    assert(cmipcb->isExit == 1);
    
    int exitCode = cmipcb->exitCode;
    uiomove(&exitCode, 4, &u);
    
    MiPCBDestroy(cmipcb);
    array_remove(curthread->t_miPCB->children, i);
    
    splx(spl);
    return 0;
    
    
}
