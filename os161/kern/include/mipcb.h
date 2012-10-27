#ifndef MIPCB_H
#define MIPCB_H

struct MiPCB {
    int processID;
    struct thread *parentThread;
    struct thread *myThread;
    struct array *children;
    int exitCode;
    
    // struct queue* openFiles;
};

#endif
