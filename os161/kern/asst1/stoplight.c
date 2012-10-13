/* 
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
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
 * Number of cars created.
 */

#define NCARS 20


/*
 *
 * Function Definitions
 *
 */

enum { NW, NE, SE, SW, NONE};
enum { N,  E,  S,  W,  EMPTY, LEAVE};

static struct {
	struct lock* intersectionLock;
	struct cv* waitForSlot[4];
    struct cv* waitForAppro[4];
    int appro[4];
	int slots[4];
} intersectionMonitor;

/*
 * the function checks deadlock when a car is approching the
 * intersection. 
 * 
 * return 0 if no deadlock, return 1 otherwise.
 * 
 */

static
void printslot()
{
	kprintf("%d %d %d %d", 	intersectionMonitor.slots[0],
							intersectionMonitor.slots[1],
							intersectionMonitor.slots[2],
							intersectionMonitor.slots[3]);
}

static int IsDeadLockOccur(int slotIndex, int direction)
{
	// assert the calling function holds the monitor lock
	assert(lock_do_i_hold(intersectionMonitor.intersectionLock));
	assert(intersectionMonitor.slots[slotIndex] == EMPTY);
	assert(slotIndex >= NW && slotIndex <= SW);
	intersectionMonitor.slots[slotIndex] = direction;
	
	// check if N = NW, E = NE, SE = S, SW = W occur.
	int i;
	for (i = 0; i < 4; i++) {
		if (intersectionMonitor.slots[i] != i) {
			intersectionMonitor.slots[slotIndex] = EMPTY;
			return 0;
		}
	}
	// kprintf("deadlock detect: ");
	// printslot();
	// kprintf(" <- ");
	intersectionMonitor.slots[slotIndex] = EMPTY;
	// printslot();
	// kprintf("\n");
	return 1;
}

static
void LeavingSlot(int slotToLeave)
{
	if (slotToLeave == NONE) return;
	lock_acquire(intersectionMonitor.intersectionLock);
	
	assert(slotToLeave >= NW && slotToLeave <= SW);
	intersectionMonitor.slots[slotToLeave] = EMPTY;
	cv_signal(intersectionMonitor.waitForSlot[slotToLeave],
				intersectionMonitor.intersectionLock);
	
	lock_release(intersectionMonitor.intersectionLock);
}

static
void Approching(int direction){
	lock_acquire(intersectionMonitor.intersectionLock);

    if (intersectionMonitor.appro[direction] == 1) {
        cv_wait(intersectionMonitor.waitForAppro[direction],
                intersectionMonitor.intersectionLock);
    }
    intersectionMonitor.appro[direction] = 1;
	lock_release(intersectionMonitor.intersectionLock);
}

static 
void GoToSlot(int curSlot, int myDirection, int slotToGo, int msg_nr, int carnumber, int src, int dest)
{
	lock_acquire(intersectionMonitor.intersectionLock);
	
	// check if deadlock occur, if so, wait
	// kprintf("slotToGo = %d\n", slotToGo);
	while (intersectionMonitor.slots[slotToGo] != EMPTY
			|| (curSlot == NONE && IsDeadLockOccur(slotToGo, myDirection))) {
		//kprintf("intersectionMonitor.slots[slotToGo] = %d\n", intersectionMonitor.slots[slotToGo]);
		cv_signal(intersectionMonitor.waitForSlot[slotToGo],
				intersectionMonitor.intersectionLock);
		cv_wait(intersectionMonitor.waitForSlot[slotToGo], 
				intersectionMonitor.intersectionLock);
	}
	
	intersectionMonitor.slots[slotToGo] = myDirection;

    assert(curSlot >= -4 && curSlot < 4);
	if (curSlot >= 0 && curSlot < NONE) {
		intersectionMonitor.slots[curSlot] = EMPTY;
	    cv_signal(intersectionMonitor.waitForSlot[curSlot],
				intersectionMonitor.intersectionLock);
	} else {
        int dirAppro = ~curSlot;
        intersectionMonitor.appro[dirAppro] = 0;
        cv_signal(intersectionMonitor.waitForAppro[dirAppro],
                intersectionMonitor.intersectionLock);
    }
	
    message(msg_nr, carnumber, src, dest);
	lock_release(intersectionMonitor.intersectionLock);
}

static const char *directions[] = { "N", "E", "S", "W" };

static const char *msgs[] = {
        "                              approaching:",
        "                       region1:    ",
        "                region2:    ",
        "        region3:    ",
        "leaving:    "
};

/* use these constants for the first parameter of message */
enum { APPROACHING, REGION1, REGION2, REGION3, LEAVING };

static void
message(int msg_nr, int carnumber, int cardirection, int destdirection)
{
	lock_acquire(intersectionMonitor.intersectionLock);
	kprintf("%s car = %2d, direction = %s, destination = %s\n",
			msgs[msg_nr], carnumber,
			directions[cardirection], directions[destdirection]);
	// printslot();
    // kprintf("\n");
	lock_release(intersectionMonitor.intersectionLock);
}
 
/*
 * gostraight()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement passing straight through the
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
gostraight(unsigned long cardirection,
           unsigned long carnumber)
{
	int slot1 = cardirection;
	int slot2 = (cardirection + 3) % 4;
	int dest = (cardirection + 2) % 4;
	
    // approching
    Approching(cardirection);
	message(APPROACHING, carnumber, cardirection, dest);
	
	// go to slot1
	GoToSlot(~cardirection, cardirection, slot1, REGION1, carnumber, cardirection, dest);
	message(REGION1, carnumber, cardirection, dest);
	
	// go to slot2
	GoToSlot(slot1, LEAVE, slot2,REGION2, carnumber, cardirection, dest);
	message(REGION2, carnumber, cardirection, dest);
	
	// leave intersection
	LeavingSlot(slot2);
	message(LEAVING, carnumber, cardirection, dest);
}


/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a left turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnleft(unsigned long cardirection,
         unsigned long carnumber)
{
	int slot1 = cardirection;
	int slot2 = (cardirection + 3) % 4;
	int slot3 = (cardirection + 2) % 4;
	
	int turn = (cardirection + 3) % 4;
	int dest = (cardirection + 1) % 4;
	
    // approching
    Approching(cardirection);
	message(APPROACHING, carnumber, cardirection, dest);
	
	// go to slot1
	GoToSlot(~cardirection, cardirection, slot1);
	message(REGION1, carnumber, cardirection, dest);
	
	// go to slot2
	GoToSlot(slot1, turn, slot2);
	message(REGION2, carnumber, cardirection, dest);
	
	// go to slot3
	GoToSlot(slot2, LEAVE, slot3);
	message(REGION3, carnumber, cardirection, dest);
	
	// leave slot3
	LeavingSlot(slot3);
	message(LEAVING, carnumber, cardirection, dest);
	
}


/*
 * turnright()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a right turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnright(unsigned long cardirection,
          unsigned long carnumber)
{
	int slot1 = cardirection;
	int dest = (cardirection + 3) % 4;
	
    // approching
    Approching(cardirection);
	message(APPROACHING, carnumber, cardirection, dest);
	
	// go to slot1
	GoToSlot(~cardirection, LEAVE, slot1);
	message(REGION1, carnumber, cardirection, dest);
	
	// leave intersection
	LeavingSlot(slot1);
	message(LEAVING, carnumber, cardirection, dest);
	
}


/*
 * approachintersection()
 *
 * Arguments: 
 *      void * unusedpointer: currently unused.
 *      unsigned long carnumber: holds car id number.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Change this function as necessary to implement your solution. These
 *      threads are created by createcars().  Each one must choose a direction
 *      randomly, approach the intersection, choose a turn randomly, and then
 *      complete that turn.  The code to choose a direction randomly is
 *      provided, the rest is left to you to implement.  Making a turn
 *      or going straight should be done by calling one of the functions
 *      above.
 */
 
static
void
approachintersection(void * unusedpointer,
                     unsigned long carnumber)
{
	(void) unusedpointer;
	int cardirection = random() % 4;
	int turn;
	
	while (1) {
		turn = random() % 4;
		if (turn == (cardirection + 2) % 4) {
			gostraight(cardirection, carnumber);
			break;
		}
		else if (turn == (cardirection + 1) % 4){
			turnleft(cardirection, carnumber);
			break;
		}
		else if (turn == (cardirection + 3) % 4) {
			turnright(cardirection, carnumber);
			break;
		}
	}
}


/*
 * createcars()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up the approachintersection() threads.  You are
 *      free to modiy this code as necessary for your solution.
 */

int
createcars(int nargs,
           char ** args)
{
	/*
	 * Avoid unused variable warnings.
	 */

	(void) nargs;
	(void) args;
	int index, error;
	/*
	 * Start NCARS approachintersection() threads.
	 */

	// initialize variables
	intersectionMonitor.intersectionLock = lock_create("");
	for (index = 0; index < 4; index++ ) {
		intersectionMonitor.waitForSlot[index] = cv_create("");
		intersectionMonitor.slots[index] = EMPTY;
        intersectionMonitor.waitForAppro[index] = cv_create("");
        intersectionMonitor.appro[index] = 0;
	}
	
	for (index = 0; index < NCARS; index++) {

			error = thread_fork("approachintersection thread",
								NULL,
								index,
								approachintersection,
								NULL
								);

			/*
			 * panic() on error.
			 */

			if (error) {
					
					panic("approachintersection: thread_fork failed: %s\n",
						  strerror(error)
						  );
			}
	}

	return 0;
}
