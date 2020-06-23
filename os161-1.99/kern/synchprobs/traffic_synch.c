#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
// static struct semaphore *intersectionSem;

// my variables

#define num_dir 4
static int cars_dist[num_dir][num_dir];

/*
 * [ [ - - - - ]
 *   [ - - - - ]
 *   [ - - - - ]
 *   [ - - - - ] ]
 */   

static struct lock *kavsak_lk;
static struct cv *kavsak_cvs[num_dir][num_dir];



static bool
turning_right(Direction org, Direction dest)
{
	if ((org == west && dest == south) || (org == south && dest == east) ||
		(org == east && dest == north) || (org == north && dest == west)) {
		return true;
	}
	return false;
}

/*
 * The conditions are the followings to make it possible to enter to kavsak
 *
 * V a and V b entered the intersection from the same direction, i.e., V a .origin = V b .origin, or
 * 
 * V a and V b are going in opposite directions, i.e., V a .origin = V b .destination and
 * V a .destination = V b .origin, or
 *
 * V a and V b have different destinations, and at least one of them is making a right turn,
 * e.g., V a is right-turning from north to west, and V b is going from south to north.
 */

// ASSGN1

static bool
pssbl2enter(Direction org, Direction dest)
{
	for (unsigned int i = 0; i < num_dir; i++) {
		for (unsigned int ii = 0; ii < num_dir; ii++) {
			if (cars_dist[i][ii] > 0) {
				if (org == i && dest == ii) { // the same dir
					continue;
				}
				else if (org == ii && dest == i) { // the opposite dir
					continue;
				}
				else if (dest != ii && turning_right(org, dest)) {
					continue;
				}
				else {
					return false;
				}
			}
		}
	}

	return true;
}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */

void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
/*
 * intersectionSem = sem_create("intersectionSem",1);
 * if (intersectionSem == NULL) {
 * panic("could not create intersection semaphore");
 * }
 * return;
 */
	// ASSGN1
		
	kavsak_lk = lock_create("kavsak_lk");
	
	KASSERT(kavsak_lk != NULL);

	for (unsigned int ii = 0; ii < num_dir; ii++) {
		for (unsigned int jj = 0; jj < num_dir; jj++) {
			kavsak_cvs[ii][jj] = cv_create("CVs of KAVSAK");

			KASSERT(kavsak_cvs[ii][jj] != NULL);

			cars_dist[ii][jj] = 0;
		}
	}
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
/*  KASSERT(intersectionSem != NULL);
 *  sem_destroy(intersectionSem);
 */
	// ASSGN1

	for (unsigned int ii = 0; ii < num_dir; ii++) {
		for (unsigned int jj = 0; jj < num_dir; jj++) {
			cv_destroy(kavsak_cvs[ii][jj]);
		}
	}

}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
 //   (void)origin;  /* avoid compiler complaint about unused parameter */
 // (void)destination; /* avoid compiler complaint about unused parameter */
 // KASSERT(intersectionSem != NULL);
 // P(intersectionSem);
 	// ASSGN1

	lock_acquire(kavsak_lk);

	while (1)
	{
		if (pssbl2enter(origin, destination) == 0) {
			cv_wait(kavsak_cvs[origin][destination], kavsak_lk);
		}
		else {
			break;
		}
	}

	cars_dist[origin][destination] = cars_dist[origin][destination] + 1;

	lock_release(kavsak_lk);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
 // * replace this default implementation with your own implementation */
 // (void)origin;  /* avoid compiler complaint about unused parameter */
 // (void)destination; /* avoid compiler complaint about unused parameter */
 // KASSERT(intersectionSem != NULL);
 // V(intersectionSem);
 	// ASSGN1

	lock_acquire(kavsak_lk);

	cars_dist[origin][destination] = cars_dist[origin][destination] - 1;

	if (cars_dist[origin][destination] == 0) {
		for (unsigned int ii = 0; ii < num_dir; ii++) {
			for (unsigned int jj = 0; jj < num_dir; jj++) {
				cv_broadcast(kavsak_cvs[ii][jj], kavsak_lk);
			}
		}
	}
	lock_release(kavsak_lk);
}
