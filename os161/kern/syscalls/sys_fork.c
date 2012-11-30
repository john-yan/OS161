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

int sys_fork(struct trapframe *tf, int* err)
{
    int spl = splhigh();
    int ret;
    
    // copy the trapframe
    struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
    if (newtf == NULL) {
        *err = ENOMEM;
        goto fail;
    }
    memmove(newtf, tf, sizeof(struct trapframe));
    
    struct addrspace *t_vmspace;
    /* Copy the address space. */
	ret = as_copy(curthread->t_vmspace, &t_vmspace);
	if (ret) {
        *err = ENOMEM;
		goto fail1;
	}
    
    // create a new thread
    struct thread *newth;
    ret = thread_fork("" /* thread name */,
			newtf /* thread arg */, (unsigned long)t_vmspace /* thread arg */,
			md_forkentry, &newth);
    if (ret) {
        *err = ENOMEM;
        goto fail2;
    }
    newth->t_vmspace = NULL;
    
    int newid = newth->t_miPCB->processID;
    splx(spl);
    
    // on success
    return newid;
    
fail2:
    as_destroy(t_vmspace);
fail1:
    kfree(newtf);
fail:
    splx(spl);
    return -1;
}

static
void md_forkentry(struct trapframe *tf, unsigned long vmspace)
{
    // disable interrupt
    struct addrspace *t_vmspace = (struct addrspace*)vmspace;
    splhigh();
    
    if (t_vmspace == NULL) {
        thread_exit();
    }
    assert(curthread->t_vmspace == NULL);
    curthread->t_vmspace = t_vmspace;
    
    
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
    as_hold(curthread->t_vmspace);
	as_activate(curthread->t_vmspace);
    
    // kprintf("sw to user mode.\n");
    
    // switch to usermode
    mips_usermode(&mytf);
    
    // should not come here
    assert(0);
}


