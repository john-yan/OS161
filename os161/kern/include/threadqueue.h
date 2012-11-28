#ifndef THREADQUEUE_H
#define THREADQUEUE_H

typedef struct {
    struct thread* head;
    struct thread* tail;
} ThreadQueue;

void TQInit(ThreadQueue* tq);
void TQAddToTail(ThreadQueue* tq, struct thread* t) ;
struct thread* TQRemoveHead(ThreadQueue* tq);
int TQIsEmpty(ThreadQueue* tq);

#endif