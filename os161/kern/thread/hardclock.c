#include <types.h>
#include <lib.h>
#include <machine/spl.h>
#include <thread.h>
#include <clock.h>

/* 
 * The address of lbolt has thread_wakeup called on it once a second.
 */
struct queue *lbolt;
ThreadQueue lboltq;
static int first = 1;

static int lbolt_counter;

/*
 * This is called HZ times a second by the timer device setup.
 */

void
hardclock(void)
{
	/*
	 * Collect statistics here as desired.
	 */


    if(first) {lbolt = q_create(1); TQInit(&lboltq);}
    first = 0;
	lbolt_counter++;
	if (lbolt_counter >= HZ) {
		lbolt_counter = 0;
		thread_wakeup(&lboltq);
	}

	thread_yield();
}

/*
 * Suspend execution for n seconds.
 */
void
clocksleep(int num_secs)
{
	int s;

    if(first) {lbolt = q_create(1); TQInit(&lboltq);}
    first = 0;
	s = splhigh();
	while (num_secs > 0) {
		thread_sleep(&lboltq);
		num_secs--;
	}
	splx(s);
}
