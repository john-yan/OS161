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

void sys__exit(int exitCode)
{
    // disable interrupt
    int spl = splhigh();
    
    curthread->t_miPCB->exitCode = exitCode;
    thread_exit();
    
    // thread_exit() should not return
    assert(0);
    
    // enable int
    splx(spl);
    
}

