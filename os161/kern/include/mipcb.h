#ifndef MIPCB_H
#define MIPCB_H

struct MiPCB {
    int processID;
    struct MiPCB *parentPCB;
    struct thread *myThread;
    struct array *children;
    struct semaphore* waitOnExit;
    int isExit;
    int exitCode;
    
    // struct queue* openFiles;
};

#endif
