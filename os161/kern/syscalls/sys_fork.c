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

static
void md_forkentry(struct trapframe *tf, unsigned long unused);

int sys_fork(struct trapframe *tf)
{
    int spl = splhigh();
    
    // copy the trapframe
    struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
    if (newtf == NULL) {
        return -1;
    }
    memmove(newtf, tf, sizeof(struct trapframe));
    
    // create a new thread
    struct thread *newth;
    int ret = thread_fork("" /* thread name */,
			newtf /* thread arg */, 1 /* thread arg */,
			md_forkentry, &newth);
    if (ret) {
        kfree(newtf);
        return -1;
    }
    
    /* Copy the address space. */
	ret = as_copy(curthread->t_vmspace, &newth->t_vmspace);
	if (ret) {
		return -1;
	}
    
    splx(spl);
    
    // on success
    return newth->t_miPCB->processID;
}

static
void md_forkentry(struct trapframe *tf, unsigned long unused)
{
    // disable interrupt
    splhigh();
    
    // copy the trapframe to our own stack and free it
    struct trapframe mytf;
    memmove(&mytf, tf, sizeof(mytf));
    kfree(tf);
    
    // set the return value to 0
    mytf.tf_v0 = 0;
    mytf.tf_a3 = 0;
    
    // next instruction
    mytf.tf_epc += 4;
    
	/* Activate it. */
	as_activate(curthread->t_vmspace);
    
    // kprintf("sw to user mode.\n");
    
    // switch to usermode
    mips_usermode(&mytf);
    
    // should not come here
    assert(0);
}


