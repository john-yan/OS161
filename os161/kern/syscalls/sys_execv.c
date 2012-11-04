#include <types.h>
#include <thread.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <vm.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <kern/callno.h>
#include <syscall.h>
#include <addrspace.h>
#include <vfs.h>
// #define MAXARGS 20
#define MAXBYTES 50

int sys_execv(struct trapframe *tf, int* err)
{
    char argv[MAXBYTES][20];
    char prog[MAXBYTES];
    int nargs = 0, i;
    size_t size, total = 0;
    userptr_t _argv = (userptr_t)tf->tf_a1;
    userptr_t _prog = (userptr_t)tf->tf_a0;
    int ret = 0;
    struct vnode *v;
	vaddr_t entrypoint, stackptr;
    struct addrspace* oldvm = curthread->t_vmspace;
    
    
    ret = copyinstr(_prog, prog, MAXBYTES, &size);
    if (ret) {
        return ret;
    }
    
    kprintf("prog: %s\n", prog);
    
    for (i = 0; i < MAXBYTES; i++) {
        char* kargv;
        if ((ret = copyin(_argv + (i<<2), &kargv, 4)) != 0) {
            return ret;
        }
        if (kargv == NULL) {
            break;
        } else {
            if ((ret = copyinstr((userptr_t)kargv, argv[i], MAXBYTES, &size)) != 0) {
                return ret;
            }
            kprintf("arg%d: %s\n", i, argv[i]);
            total += size;
        }
        nargs++;
    }
    
    /* Open the file. */
	if ((ret = vfs_open(prog, O_RDONLY, &v)) != 0) {
        kprintf("can't open file.");
		return ret;
	}
    
    int spl = splhigh();
    /* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
        kprintf("can't create new vm.");
        curthread->t_vmspace = oldvm;
		vfs_close(v);
        splx(spl);
		return ENOMEM;
	}
    
    /* Activate it. */
	as_activate(curthread->t_vmspace);
    
    /* Load the executable. */
	ret = load_elf(v, &entrypoint);
    vfs_close(v);
	if (ret) {
        kprintf("can't load elf.");
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = oldvm;
        as_activate(curthread->t_vmspace);
        splx(spl);
		return ret;
	}
    
    /* Define the user stack in the address space */
	if ((ret = as_define_stack(curthread->t_vmspace, &stackptr))) {
        kprintf("can't define stack.");
		as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = oldvm;
        as_activate(curthread->t_vmspace);
        splx(spl);
		return ret;
	}
    
    char *uargv[nargs + 1];
    uargv[nargs] = NULL;
    stackptr -= (total/4 + 1) * 4;
    userptr_t curptr = stackptr;
    
    // kprintf("stackptr = %x, total = %d\n", stackptr, total);
    for (i = 0; i < nargs; i++) {
        uargv[i] = curptr;
        if ((ret = copyoutstr(argv[i], curptr, MAXBYTES, &size)) != 0) {
            as_destroy(curthread->t_vmspace);
            curthread->t_vmspace = oldvm;
            as_activate(curthread->t_vmspace);
            splx(spl);
            return ret;
        }
        curptr += size;
    }
    
    stackptr -= (nargs + 1)<<2;
    curptr = stackptr;
    
    if ((ret = copyout(uargv, curptr, (nargs + 1)<<2)) != 0) {
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = oldvm;
        as_activate(curthread->t_vmspace);
        splx(spl);
        return ret;
    }
    
    // kprintf("stackptr = %x\n", stackptr);
    // for (i = 0; i < nargs; i++) {
        // kprintf("argv[%d]: %x\n", i, uargv[i]);
    // }
    
    as_destroy(oldvm);
    
    /* Warp to user mode. */
	md_usermode(nargs /*argc*/, curptr /*userspace addr of argv*/,
		    stackptr, entrypoint);
    
    return 0;
}






















