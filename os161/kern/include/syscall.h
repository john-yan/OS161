#ifndef _SYSCALL_H_
#define _SYSCALL_H_

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */
 
void sys__exit(int exitCode);
int sys_waitpid(int pid, int* status, int options);
int sys_getpid();
int sys_reboot(int code);
int sys_fork(struct trapframe *tf, int* err);
int sys_execv(struct trapframe *tf, int* err);
int sys_sbrk(int bytes);

#endif /* _SYSCALL_H_ */
