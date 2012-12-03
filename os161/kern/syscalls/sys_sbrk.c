 
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

int sys_sbrk(int bytes)
{
    assert (curthread->t_vmspace != NULL);
    
    int result;
    
    if (bytes < 0) return -1;
    
    result = as_heap(curthread->t_vmspace, bytes);
    
    return result;
}
