/*
 * catsem.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use SEMAPHORES to solve the cat syncronization problem in 
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

static int bowls[] = {0, 0};
static int ncats_eating = 0;
static int nmice_eating = 0;

static struct semaphore *sem;
static struct semaphore *bowl_sem;
static struct semaphore *bowl;

static
int getbowl()
{
	int i;
    P(bowl_sem);
    if (bowls[0] == 0) {
        bowls[0] = 1;
        i = 0;
    } else if (bowls[1] == 0){
        bowls[1] = 1;
        i = 1;
    } else {
        panic("can't find a empty bowl.\n");
        i = -1;
    }
    V(bowl_sem);
	return i;
}

static 
void retbowl(int i)
{
    P(bowl_sem);
    if (bowls[i] == 1) {
        bowls[i] = 0;
    } else {
        panic("the bowl isn't held by anyone.\n");
    }
    V(bowl_sem);
}
/*
 * 
 * Function Definitions
 * 
 */

/* who should be "cat" or "mouse" */
static void
sem_eat(const char *who, int num, int bowl, int iteration)
{
        kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
		clocksleep(1);
        kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
}

/*
 * catsem()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using semaphores.
 *
 */

static
void
catsem(void * unusedpointer, 
       unsigned long catnumber)
{
        /*
         * Avoid unused variable warnings.
         */

        (void) unusedpointer;
        (void) catnumber;
        int i, mybowl;
		kprintf("cat %d start\n", catnumber);
        for(i = 0; i < 4; i++){
            P(bowl);

            P(sem);
            
            while(1) {
                if (nmice_eating != 0){
                    V(sem);
                    thread_yield();
                    P(sem);
                } else {
					ncats_eating++;
                    break;
                }
            }
            V(sem);

            mybowl = getbowl();
            sem_eat("cat", catnumber, mybowl, i);

            P(sem);
            ncats_eating--;
            V(sem);

            retbowl(mybowl);
            V(bowl);
        }
}
        

/*
 * mousesem()
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
 *      Write and comment this function using semaphores.
 *
 */

static
void
mousesem(void * unusedpointer, 
         unsigned long mousenumber)
{
        /*
         * Avoid unused variable warnings.
         */

        (void) unusedpointer;
        (void) mousenumber;
        int i, mybowl;
        for(i = 0; i < 4; i++){
            P(bowl);

            P(sem);
            while (1){
                if (ncats_eating != 0){
                    V(sem);
                    thread_yield();
                    P(sem);
                } else {
                    nmice_eating++;
                    break;
                }
            }
            V(sem);

            mybowl = getbowl();
            sem_eat("mouse", mousenumber, mybowl, i);

            P(sem);
            nmice_eating--;
            V(sem);

            retbowl(mybowl);
            V(bowl);
        }
}


/*
 * catmousesem()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catsem() and mousesem() threads.  Change this 
 *      code as necessary for your solution.
 */

int total = 0;
static
void test(void* v, int k)
{
    int i, j;
    for(i = 0; i < 5; i++){
        thread_yield();
        P(sem);
        thread_yield();
        kprintf("\n");
        for(j = 0; j < k + 1; j++){
            thread_yield();
            kprintf("   ");
            thread_yield();
        }
        thread_yield();
        kprintf("%d", i);
        thread_yield();
        V(sem);
    }
}

int
catmousesem(int nargs,
            char ** args)
{
        int index, error;
   
        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;
   
        /*
         * Start NCATS catsem() threads.
         */
        sem = sem_create("sem", 1);
        bowl = sem_create("bowl", 2);
        bowl_sem = sem_create("bowl_sem", 1);
        
        for (index = 0; index < 1000; index++)
            thread_yield();


        //for (index = 0; index < 10; index ++) {
        //    thread_fork("", NULL, index, test, NULL);
        //}

        //return 0;

        for (index = 0; index < NCATS; index++) {
           
                error = thread_fork("catsem Thread", 
                                    NULL, 
                                    index, 
                                    catsem, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
                 
                        panic("catsem: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }
        
        /*
         * Start NMICE mousesem() threads.
         */

        for (index = 0; index < NMICE; index++) {
   
                error = thread_fork("mousesem Thread", 
                                    NULL, 
                                    index, 
                                    mousesem, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
         
                        panic("mousesem: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

        return 0;
}


/*
 * End of catsem.c
 */
