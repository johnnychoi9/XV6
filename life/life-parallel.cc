#include "life.h"
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <string>
#include <functional>

//two functions are needed to maintain separation of running the algo and managing threads
//I also need some sort of a struct to keep stuff attached to a particular thread. 
//similar to the one out of the slides for thread info
struct threadinfo{
	LifeBoard * currBoard;
	LifeBoard * nextBoardEven;
	LifeBoard * nextBoardOdd;
	pthread_barrier_t * stopIt;
	int startingPoint;
//	int generation; this got replaced by just using step
	int endingPoint;
	int stepcount;
};

void * life_section(void *theboisInfo);

// same structure as in the slides
void simulate_life_parallel(int threads, LifeBoard &state, int steps) {
    /* YOUR CODE HERE */
	//initialize everything that needs to go into the struct
	pthread_barrier_t barr;
	LifeBoard curr_state(state);
	LifeBoard next_state_Odd(state.width(), state.height());
	LifeBoard next_state_Even(state.width(), state.height());
	//make all the threads
	pthread_t * thebois = new pthread_t[threads];
	threadinfo * theboisInfo = new threadinfo[threads]; 
	int curr = 1;
	int k = 0;
	// know how big to make all the swaths of path
	for(int x = 0; x < state.height(); x+=threads){
		k++;
	}
	//initialize barrier (this gets turned into stopper in the actual function
	pthread_barrier_init(&barr, NULL, (unsigned)threads);
	for(int x = 0; x < threads; x++){
		theboisInfo[x].currBoard = &curr_state;
		theboisInfo[x].nextBoardOdd = &next_state_Odd;
		theboisInfo[x].nextBoardEven = &next_state_Even;
		//theboisInfo[x].generation = 0;
		theboisInfo[x].startingPoint = curr;
		if(x == threads -1){
			theboisInfo[x].endingPoint = state.height() -1;
		}else{
		theboisInfo[x].endingPoint = curr + k;
		}
		curr += k;
		theboisInfo[x].stepcount = steps;
		// all of our thread have to modify the same boards
		theboisInfo[x].stopIt = &barr;
	}
	//make all of the threads for us to use, xt is for thread at a given x
	for(int xt = 0; xt < threads; xt++){
		pthread_create(&thebois[xt], NULL, life_section, (void*)&theboisInfo[xt]);
	}
	//join everything back together
	for(int xt = 0; xt < threads; xt++){
		pthread_join(thebois[xt], NULL);
	}
	//delete everything 
	delete[] theboisInfo;
	delete[] thebois;
	pthread_barrier_destroy(&barr);
	state = curr_state;
	return;
}
// handle the threads execution (same structure as the slides)
void * life_section(void *theboisInfo){
	struct threadinfo * theBoiInfo = (struct threadinfo *) theboisInfo; // info about our guys
	for (int step = 0; step < theBoiInfo->stepcount; step++){
		for(int y = theBoiInfo->startingPoint; y < theBoiInfo->endingPoint; y++){
			for(int x = 1; x < theBoiInfo->currBoard->width() - 1; x++){ // same code as down below
				int live_in_window = 0;
				for (int y_offset = -1; y_offset <= 1; y_offset++){
					for( int x_offset = -1; x_offset <= 1; x_offset++){
						if(theBoiInfo->currBoard->at(x + x_offset, y + y_offset)){
							live_in_window++;
						}
					}
				}
				// depending on which generation it is, write to either nextBoardOdd or nextBoardEven
				if(step % 2 != 0){
				theBoiInfo->nextBoardOdd->at(x, y) = (live_in_window == 3 || (live_in_window == 4 && theBoiInfo->currBoard->at(x,y)));
				}else{
				theBoiInfo->nextBoardEven->at(x, y) = (live_in_window == 3 || (live_in_window == 4 && theBoiInfo->currBoard->at(x,y)));	
				}
			}
		}
		//wait for shit to finish
		int stopper = pthread_barrier_wait(theBoiInfo->stopIt);
		//Use either the even or the odd board depending on the step count
		if(stopper != 0){
			if(step % 2 != 0)
				swap(*(theBoiInfo->currBoard), *(theBoiInfo->nextBoardOdd));
			else
				swap(*(theBoiInfo->currBoard), *(theBoiInfo->nextBoardEven));
		}
		pthread_barrier_wait(theBoiInfo->stopIt);
	}
	return 0;
}	
/*void simulate_life_serial(LifeBoard &state, int steps) {
    LifeBoard next_state{state.width(), state.height()};
    for (int step = 0; step < steps; ++step) {*/
        /* We use the range [1, width - 1) here instead of
         * [0, width) because we fix the edges to be all 0s.
         */
        /*for (int y = 1; y < state.height() - 1; ++y) {
            for (int x = 1; x < state.width() - 1; ++x) {
                int live_in_window = 0;*/
                /* For each cell, examine a 3x3 "window" of cells around it,
                 * and count the number of live (true) cells in the window. */
                /*for (int y_offset = -1; y_offset <= 1; ++y_offset) {
                    for (int x_offset = -1; x_offset <= 1; ++x_offset) {
                        if (state.at(x + x_offset, y + y_offset)) {
                            ++live_in_window;
                        }
                    }
                }*/
                /* Cells with 3 live neighbors remain or become live.
                   Live cells with 2 live neighbors remain live. */
                /*next_state.at(x, y) = (
                    live_in_window == 3  dead cell with 3 neighbors or live cell with 2 ||
                   (live_in_window == 4 && state.at(x, y)) 
                );
            }
        }
        */
        /* now that we computed next_state, make it the current state */
        /*swap(state, next_state);
    }
}*/