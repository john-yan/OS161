/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>
#include <queue.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

    // sem->waitqueue = q_create(1);
	sem->count = initial_count;
    TQInit(&sem->tq);
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(!thread_hassleepers(&sem->tq));
    assert(TQIsEmpty(&sem->tq));
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

    // q_destroy(sem->waitqueue);
	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(&sem->tq);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(&sem->tq);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
    lock->holdingThread = NULL;
    // lock->waitqueue = q_create(5);
    // if (lock->waitqueue == NULL) {
        // kfree(lock->name);
        // kfree(lock);
        // return NULL;
    // }
    
    TQInit(&lock->tq);
	// add stuff here as needed
	
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);
    assert(lock->holdingThread == NULL);
    // assert(q_empty(lock->waitqueue));
    assert(TQIsEmpty(&lock->tq));
    
	// add stuff here as needed
	// q_destroy(lock->waitqueue);
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

    // disable interrupt
    int spl;
    spl = splhigh();

    if (lock->holdingThread != curthread) {
        // if the current thread is the holding thread,
        // then nothing needed to be done.
        
        // if the holding thread is NULL,
        // then we can hold it and return.
        // otherwise, someone has already held it,
        // then we go to sleep.
        while (lock->holdingThread != NULL){
            // TQAddToTail(&lock->tq, curthread);
            thread_sleep(&lock->tq);
        }

        lock->holdingThread = curthread;
    }
    // enable interrupt
    splx(spl);
}

void
lock_release(struct lock *lock)
{
    assert(lock != NULL);
    assert(lock->holdingThread == curthread);

    int spl;

    // disable int
    spl = splhigh();

    // first release the lock by setting holdingThread to NULL
    // then wakeup one thread (if has any) in the waitqueue.
    lock->holdingThread = NULL;
    thread_wakeup(&lock->tq);

    // enable int
    splx(spl);
}

int
lock_do_i_hold(struct lock *lock)
{
    assert(lock != NULL);

    return lock->holdingThread == curthread;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
    // cv->waitqueue = q_create(5);
    // if (cv->waitqueue == NULL) {
        // kfree(cv->name);
        // kfree(cv);
        // return NULL;
    // }
	
    TQInit(&cv->tq);
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

    // q_destroy(cv->waitqueue);
	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    assert(cv != NULL);
    assert(lock != NULL);

    int spl;

    // disable int
    spl = splhigh();

    // we must own the lock
    assert(lock->holdingThread == curthread);
    
    lock_release(lock);
    thread_sleep(&cv->tq);
    lock_acquire(lock);
    
    // enable int
    splx(spl);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    assert(cv != NULL);
    assert(lock != NULL);

    int spl;
    // disable int
    spl = splhigh();

    // if (!q_empty(cv->waitqueue)) {
        thread_wakeup( &cv->tq);
    // }

    // enable int
    splx(spl);

}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    assert(cv!=NULL);
    assert(lock!=NULL);

    int spl;

    // disable int
    spl = splhigh();
    
    thread_wakeupAll(&cv->tq);

    // enable int
    splx(spl);
}
