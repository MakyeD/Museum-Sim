#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "semaphore.h"
#include "museumsim.h"

//
// In all of the definitions below, some code has been provided as an example
// to get you started, but you do not have to use it. You may change anything
// in this file except the function signatures.
//


struct shared_data {
	// Add any relevant synchronization constructs and shared state here.
	// For example:
	//     pthread_mutex_t ticket_mutex;
	//     int tickets;

	pthread_mutex_t ticket_mutex;
	pthread_mutex_t visitor_enter_mutex;
	pthread_mutex_t visitor_leaves_mutex;
	pthread_mutex_t guide_enters_mutex;
	
	sem_t guides_max;
	sem_t visitors_waiting;
	sem_t visitors_admit;
	sem_t guides_can_leave;
	sem_t guide_chill;

	int tix_used;
	int waiting;
	int guides_inside;
	int visitors_inside;
	int guides_finished;
	int visitors_left;
	int visitors_this_run;
	int guides_initialized;
	int total_visitors_admitted;
	int toured_and_left;
	int tickets;
};

static struct shared_data shared;


/**
 * Set up the shared variables for your implementation.
 * 
 * `museum_init` will be called before any threads of the simulation
 * are spawned.
 */
void museum_init(int num_guides, int num_visitors)
{
	// pthread_mutex_init(&shared.ticket_mutex, NULL);
	//

	shared.tickets = MIN(VISITORS_PER_GUIDE * num_guides, num_visitors);
	
	pthread_mutex_init(&shared.ticket_mutex, NULL);
	pthread_mutex_init(&shared.visitor_enter_mutex, NULL);
	pthread_mutex_init(&shared.visitor_leaves_mutex, NULL);
	pthread_mutex_init(&shared.guide_enters_mutex, NULL);
	sem_init(&shared.guides_max, 0, GUIDES_ALLOWED_INSIDE);
	sem_init(&shared.visitors_waiting, 0, 0);
	sem_init(&shared.visitors_admit, 0,0);
	sem_init(&shared.guides_can_leave, 0 , 0);
	sem_init(&shared.guide_chill,0,0);

	//used
	shared.tix_used = 0;
	shared.waiting = 0;
	shared.guides_inside = 0;
	shared.visitors_inside = 0;
	shared.guides_finished = 0;
	shared.visitors_left = 0;
	shared.visitors_this_run = num_visitors;
	shared.guides_initialized = num_guides;
	shared.total_visitors_admitted = 0;
	shared.toured_and_left = 0;
}


/**
 * Tear down the shared variables for your implementation.
 * 
 * `museum_destroy` will be called after all threads of the simulation
 * are done executing.
 */
void museum_destroy()
{
	// pthread_mutex_destroy(&shared.ticket_mutex);
	pthread_mutex_destroy(&shared.ticket_mutex);
	pthread_mutex_destroy(&shared.visitor_enter_mutex);
	pthread_mutex_destroy(&shared.visitor_leaves_mutex);
	pthread_mutex_destroy(&shared.guide_enters_mutex);
	sem_destroy(&shared.guides_max);
	sem_destroy(&shared.visitors_waiting);
	sem_destroy(&shared.visitors_admit);
	sem_destroy(&shared.guides_can_leave);

	}


/**
 * Implements the visitor arrival, touring, and leaving sequence.
 */
void visitor(int id)
{
	// visitor_arrives(id);
	// visitor_tours(id);
	// visitor_leaves(id);

	visitor_arrives(id);

	//Check if there are tickets available
	if(shared.tix_used == shared.tickets){
		//if there are, visitor leaves
		visitor_leaves(id);
		return;
	
	}

	pthread_mutex_lock(&shared.ticket_mutex);
	
	//Otherwise increase amount of tickets in use
	shared.tix_used++;

	//Add one to the line wait
	shared.waiting++;

	pthread_mutex_unlock(&shared.ticket_mutex);

	//Increment visitors waiting in line, guide will decrement
	sem_post(&shared.visitors_waiting);

	pthread_mutex_lock(&shared.ticket_mutex);
	//Leaves line
	shared.visitors_left++;
	//waits until a tour guide lets visitor in, decrements 

	pthread_mutex_unlock(&shared.ticket_mutex);
	
	//Visitor goes inside
	sem_wait(&shared.visitors_admit);

	visitor_tours(id);

	visitor_leaves(id);	

	pthread_mutex_lock(&shared.ticket_mutex);
	shared.toured_and_left++;
	pthread_mutex_unlock(&shared.ticket_mutex);


	pthread_mutex_lock(&shared.visitor_leaves_mutex);

	//Visitor leaves
	shared.visitors_inside--;
	shared.visitors_left--;
	pthread_mutex_unlock(&shared.visitor_leaves_mutex);

	//Check for cases w/more than 2 guides
	if(shared.guides_initialized > 2){	
		
		//If total guides that have finished < 2
		if(shared.guides_finished < 2){	
			
			//Check if the visitors that have toured and left is equal to the limit
			if(shared.toured_and_left == GUIDES_ALLOWED_INSIDE * VISITORS_PER_GUIDE){

				//increment the semaphore by the amount of guides inside
				int i = 0;
				while(i<shared.guides_inside){

					sem_post(&shared.guides_can_leave);
					i++;
				}
			}

		//If the total number of guides that have finished is >=2  and < 5
		}else if(shared.guides_finished >= 2 && shared.guides_finished <=4){	

			//Check if we've passed all our visitors through
			if(shared.toured_and_left == shared.total_visitors_admitted || shared.toured_and_left == shared.guides_initialized * VISITORS_PER_GUIDE){

				int i = 0;
				while(i<shared.guides_inside){

					sem_post(&shared.guides_can_leave);
					i++;
				}
			}
			
		}	
	
	//For cases with less than 3 guides
	}else{

		if(shared.visitors_left == 0){

			int i = 0;
			while(i<shared.guides_inside){

				sem_post(&shared.guides_can_leave);
				i++;
			}

		}

	}


	return;
}

/**
 * Implements the guide arrival, entering, admitting, and leaving sequence.
 */
void guide(int id)
{

	// guide_arrives(id);
	// guide_enters(id);
	// guide_admits(id);
	// guide_leaves(id);

	guide_arrives(id);	

	sem_wait(&shared.guides_max);

	guide_enters(id);

	pthread_mutex_lock(&shared.guide_enters_mutex);
	shared.guides_inside++;
	pthread_mutex_unlock(&shared.guide_enters_mutex);

		//Keep track of this guide's visitors
		int this_guides_visitors = 0;

		//While there less visitors for this guide than the limit...
		pthread_mutex_lock(&shared.visitor_enter_mutex);
		while(this_guides_visitors < VISITORS_PER_GUIDE && this_guides_visitors < shared.visitors_this_run && (shared.total_visitors_admitted < shared.visitors_this_run)){

				sem_wait(&shared.visitors_waiting);
				sem_post(&shared.visitors_admit);

				guide_admits(id);
				this_guides_visitors++;
				shared.visitors_inside++;
				shared.total_visitors_admitted++;
				shared.waiting--;

		}

		sem_post(&shared.guide_chill);	

		pthread_mutex_unlock(&shared.visitor_enter_mutex);

		//If all visitors gone, continues
		sem_wait(&shared.guides_can_leave);

		pthread_mutex_lock(&shared.guide_enters_mutex);
		shared.guides_inside--;
		shared.guides_finished++;
		pthread_mutex_unlock(&shared.guide_enters_mutex);

		
		//If there is < 1 shared guide inside
		if(shared.guides_inside > 0){

			//block until other guide exits
			sem_wait(&shared.guide_chill);
			sem_wait(&shared.guide_chill);
			

		}else{

			//For the last guide that exits
			guide_leaves(id);
			sem_post(&shared.guides_max);
			sem_post(&shared.guides_max);
			return;

		}

		guide_leaves(id);
			

	return;


	
}
