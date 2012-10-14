/*
 * catlock.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use LOCKS/CV'S to solve the cat syncronization problem in 
 * this file.
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>


/*
 * 
 * Constants
 *
 */

/*
 * Number of food bowls.
 */

#define NFOODBOWLS 2

/*
 * Number of cats.
 */

#define NCATS 6

/*
 * Number of mice.
 */

#define NMICE 2

static struct cv* waitForNoThreatOrBowl;
static struct lock* mon;
static int numOfMice = 0, numOfCat = 0;

static int bowls[] = {0, 0};

static
int hasbowl()
{
	return bowls[0] == 0 || bowls[1] == 0;
}

static
int getbowl()
{
    if (bowls[0] == 0) {
        bowls[0] = 1;
        return 0;
    } else if (bowls[1] == 0){
        bowls[1] = 1;
        return 1;
    } else {
        return -1;
    }
}

static 
void retbowl(int i)
{
    if (bowls[i] == 1) {
        bowls[i] = 0;
    } else {
        panic("the bowl isn't held by anyone.\n");
    }
}
/*
 * 
 * Function Definitions
 * 
 */

/* who should be "cat" or "mouse" */
static void
lock_eat(const char *who, int num, int bowl, int iteration)
{
        kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
		int i;
		for (i = 0; i < 100; i ++)
			thread_yield();
        // clocksleep(1);
        kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
}

/*
 * catlock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS -
 *      1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
catlock(void * unusedpointer, 
        unsigned long catnumber)
{
        /*
         * Avoid unused variable warnings.
         */

        (void) unusedpointer;
        (void) catnumber;
		int i;
		int mybowl;
		// kprintf("cat %d start\n", catnumber);
		for(i = 0; i < 4; i++) {
			lock_acquire(mon);
			while(1){
				if (numOfMice != 0 ||  !hasbowl())
					cv_wait(waitForNoThreatOrBowl, mon);
				else
					break;
			}
			numOfCat ++;
			mybowl = getbowl();
			lock_release(mon);
			
			lock_eat("cat", catnumber, mybowl + 1, i);
			
			lock_acquire(mon);
			retbowl(mybowl);
			numOfCat --;
			cv_signal(waitForNoThreatOrBowl, mon);
			lock_release(mon);
		}
}
	

/*
 * mouselock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds the mouse identifier from 0 to 
 *              NMICE - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
mouselock(void * unusedpointer,
          unsigned long mousenumber)
{
        /*
         * Avoid unused variable warnings.
         */
        
        (void) unusedpointer;
        (void) mousenumber;
		
		int i;
		int mybowl;
		for(i = 0; i < 4; i++) {
			lock_acquire(mon);
			while(1){
				if (numOfCat != 0 || !hasbowl())
					cv_wait(waitForNoThreatOrBowl, mon);
				else
					break;
			}
			numOfMice ++;
			mybowl = getbowl();
			lock_release(mon);
			
			lock_eat("mouse", mousenumber, mybowl + 1, i);
			
			lock_acquire(mon);
			retbowl(mybowl);
			numOfMice --;
			cv_signal(waitForNoThreatOrBowl, mon);
			lock_release(mon);
		}
		
}


/*
 * catmouselock()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catlock() and mouselock() threads.  Change
 *      this code as necessary for your solution.
 */

int
catmouselock(int nargs,
             char ** args)
{
        int index, error;
   
        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;
   
        /*
         * Start NCATS catlock() threads.
         */
		 
		// l = lock_create("");
		// for (index = 0; index < 2000; index++)
            // thread_yield();


        // for (index = 0; index < 10; index ++) {
           // thread_fork("", NULL, index, test, NULL);
        // }

        // return 0;
		
		mon = lock_create("");
		waitForNoThreatOrBowl = cv_create("");
		
        for (index = 0; index < NCATS; index++) {
           
                error = thread_fork("catlock thread", 
                                    NULL, 
                                    index, 
                                    catlock, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
                 
                        panic("catlock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

        /*
         * Start NMICE mouselock() threads.
         */

        for (index = 0; index < NMICE; index++) {
   
                error = thread_fork("mouselock thread", 
                                    NULL, 
                                    index, 
                                    mouselock, 
                                    NULL
                                    );
      
                /*
                 * panic() on error.
                 */

                if (error) {
         
                        panic("mouselock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

        return 0;
}

/*
 * End of catlock.c
 */
