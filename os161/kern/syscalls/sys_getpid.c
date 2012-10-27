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

int sys_getpid()
{
    // disable interrupt
    int spl = splhigh();
    
    int myid = curthread->t_miPCB->processID;
    
    // enable int
    splx(spl);
    
    return myid;
}

