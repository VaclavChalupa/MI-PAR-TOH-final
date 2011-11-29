/*
 * processor.c
 *
 */

/*  */
#define CONTROL_MESSAGE_FLAG 100

/* process color */
#define BLACK_COLOR 1
#define WHITE_COLOR 0

/* process state */
#define ACTIVE 1
#define IDLE 0

/* init */
#define INIT 1
#define RUNNING 0


/* message types */
#define MSG_INIT 100
#define MSG_NEW_MIN_STEPS 150
#define MSG_TOKEN 900
#define MSG_WORK 250
#define MSG_NO_WORK 220
#define MSG_END 200
#define MSG_EMERGENCY_END 500
#define MSG_SOLUTION_INFO 111
#define MSG_FINAL_PROCESSOR 152
#define MSG_WANT_WORK 569

/* deleni zasobniku - rezna hladina */
#define H 10

#include "state_printer.h"
#include "tower.h"
#include "stack_item.h"
#include "stack.h"
#include "process_item.h"
#include "analyser.h"
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include "debug.h"
#include <time.h>


/* MPI INFO */
int process_id;
int processors;
short state = IDLE;
short color = WHITE_COLOR;
short global_state;

/* solution info */
short towersCount;
short discsCount;
short min;
short max;
short minSteps;
short destTower;
short processorWithSolution = -1;
struct {
	ProcessItem *head;
} solutionQueue;

/* stack */
Stack * stack;

short initData[5];

static void describeMove(short* prevState, short* currentState, short* disc, short* sourceTower, short* destTower);
static int compareStates(short* prevState, short* currentState);
static void inspectStack();
static short*  serializeState(Tower* _towers);
static Tower* deserializeState(short* data);
static int loopDetected();
static void run();
static void freeInspectStack();
static void init(short _size, short _discsCount, short _destTower, short _min, short _max);
static void printSolutionIfExists();
static void deserializeStack(short* stackData, short itemsCount);
static short*  serializeStack(short* count);

short*  serializeState(Tower* _towers) {
	short * stack_item_data, i;
	stack_item_data = (short*) malloc(discsCount * sizeof(short));

	for(i = 0; i < discsCount; i++) {
		stack_item_data[i] = -1;
	}

	for(i = 0; i < towersCount; i++) {
		Disc* disc;
		disc = _towers[i].top;
		while(disc != NULL) {
			stack_item_data[disc->size-1] = i; /* disc size -> tower indexed form 0*/
			disc = disc->next;
		}
	}

	for(i = 0; i < discsCount; i++) {
		if(stack_item_data[i] == -1) {
			debug_print("ERROR: stack_item_data defect by serialization for i = %i, process %i", i, process_id);
			return NULL;
		}
	}

	return stack_item_data;
}

Tower* deserializeState(short* data) {
	int i;
	Tower* _towers;

	_towers = (Tower*) malloc (towersCount * sizeof(Tower));

	for(i = 0; i < towersCount; i++) {
		_towers[i].number = i+1;
		_towers[i].top = NULL;
	}

	for(i = discsCount-1; i >= 0; i--) {
		insertDics(i+1, &_towers[data[i]]);
	}

	return _towers;
}

int loopDetected() {
	StackItem * current, * tmp;
	int loop;
	current = stack->top;

	if(stack == NULL) {
		return 0;
	}

	for(tmp = current->next, loop = 0; tmp && loop < 20000; tmp = tmp->next, loop++) {
		if(compareStates(current->data, tmp->data)) {
			return 1;
		}
	}
	return 0;
}

void deserializeStack(short* stackData, short itemsCount) {

	short offset, bulk;
	bulk = discsCount + 6;


	/*for(int i = 0; i < itemsCount; i++) {
		debug_print("****RECEIVING  (process %i) DATA[%i] = %i", process_id, i, stackData[i]);
	}*/

	for (offset = 0; offset < itemsCount; offset += bulk) {
		short* data;
		short step, i, j, movedDisc, received, sent;
		short disc;
		data = (short*) malloc(discsCount * sizeof(short));


		for (disc = 0; disc < discsCount; disc++) {
			data[disc] = stackData[offset + disc];
		}
		step = stackData[offset + discsCount];
		i = stackData[offset + discsCount + 1];
		j = stackData[offset + discsCount + 2];
		movedDisc = stackData[offset + discsCount + 3];
		received = stackData[offset + discsCount + 4];
		sent = stackData[offset + discsCount + 5];

		push(data, step, i, j, movedDisc, 1, 1);
	}
	stack->top->received = 0;
	stack->top->sent = 0;
	stack->num = 1;
}


short* serializeStack(short* count) {
	int cut;
	short* stackData = NULL;
	StackItem * tmp;
	StackItem* item;
	short offset = 0;
	short foundLevel = 0;
	short counter = 0;

	*count = 0;
	cut = stack->num - H;

	if(!isStackEmpty() && cut > 0) {
		/* found stack item in H-- */
		int level = stack->num;
		tmp = stack->top;
		for(tmp = tmp->next; tmp; tmp = tmp->next) {
			level--;
			/* send only perspective branch */
			if(tmp->i < towersCount-1 && level <= cut && level >= 1 && tmp->sent != 1) {
				foundLevel = 1;
				break;
			}
			if(level < 1) {
				break;
			}
		}

		/* prepracovat deleni */
		if (foundLevel == 1) {
			item = tmp;
			while (item != NULL) {
				counter += discsCount + 6;
				item = item->next;
			}

			stackData = (short*) malloc(counter * sizeof(short));
			item = tmp;
			while (item != NULL) {
				int i;
				for (i = 0; i < discsCount + 6; i++) {
					if (i < discsCount) {
						stackData[offset + i] = item->data[i];
					} else if (i == discsCount) {
						stackData[offset + i] = item->step;
					} else if (i == discsCount + 1) {
						stackData[offset + i] = item->i;
					} else if (i == discsCount + 2) {
						stackData[offset + i] = item->j;
					} else if (i == discsCount + 3) {
						stackData[offset + i] = item->movedDisc;
					} else if (i == discsCount + 4) {
						stackData[offset + i] = item->received;
					} else if (i == discsCount + 5) {
						stackData[offset + i] = item->sent;
					}
				}
				offset += (discsCount + 6);
				item = item->next;
			}
			/* set as sent */
			tmp->sent = 1;
			/*debug_print("WWWWWWW (process %i) SEND = %i, num= %i, i = %i, j = %i, sent = %i", process_id, tmp->received, stack->num, tmp->i, tmp->j, tmp->sent);*/

			/*for(int i = 0; i < counter; i++) {
				debug_print("****SENDING  (process %i) DATA[%i] = %i", process_id, i, stackData[i]);
			}*/


		}
		*count = counter;
		return stackData;
	}
	return stackData;
}

void run() {
	short workRequestProcessor = 0; // request root
	short askewForWork = 0;
	int counter = 0;
	short breakProcess = 0;
	short newMinSteps;
	short isMessage;

	short dev_null = 0;

	/* MPI */
	int flag;
	MPI_Status status;

	/* token processing */
	int hasToken = 0;
	short receivedTokenColor;
	int tokenSent = 0;

	int i;

	while(breakProcess == 0) {

		counter++;

		/* init others */
		if(state == IDLE && process_id == 0 && counter > (processors + H)) {
			/* send info for others:
			 *
			 * * towersCount
			 * * discCount
			 * * destTower
			 * * min
			 * * max
			 *
			 **/

			initData[0] = towersCount;
			initData[1] = discsCount;
			initData[2] = destTower;
			initData[3] = min;
			initData[4] = max;

			for (i = 1; i < processors; i++) {
				MPI_Send(initData, 5, MPI_SHORT, i, MSG_INIT, MPI_COMM_WORLD);
			}
			state = ACTIVE;
		}


		/* if root and no Work -> send token */
		if((isStackEmpty() && global_state == RUNNING) && (counter % (CONTROL_MESSAGE_FLAG)) == 0) {

			/* want work */
			if(isStackEmpty() && processors > 1 && askewForWork == 0 && global_state == RUNNING) {

				askewForWork = 1;

				debug_print("REQUESTING WORK (process %i) TO PROCESS %i", process_id, workRequestProcessor);
				MPI_Send(&workRequestProcessor, 1, MPI_SHORT, workRequestProcessor, MSG_WANT_WORK, MPI_COMM_WORLD);


				if(state == IDLE) {
					/*initialized non root - continue right work request*/
					state = ACTIVE;
					workRequestProcessor = (process_id + 1) % processors;
				} else {
					workRequestProcessor = (workRequestProcessor + 1) % processors;
					if(workRequestProcessor == process_id) {
						workRequestProcessor = (workRequestProcessor + 1) % processors;
					}
				}

				/*srand (time(NULL));
				workRequestProcessor = rand() % processors;*/

			}

			if(isStackEmpty() && process_id == 0 && tokenSent == 0) {
				if(processors > 1) {
					debug_print("MASTER (process %i) IS SENDING TOKEN", process_id);
					color = WHITE_COLOR;
					tokenSent = 1;
					MPI_Send(&color, 1, MPI_SHORT, 1, MSG_TOKEN, MPI_COMM_WORLD);
				} else {
					breakProcess = 1;
				}
		    } else if (isStackEmpty() && process_id != 0 && hasToken == 1) {
		    	color = WHITE_COLOR;
		    	hasToken = 0;
		    	debug_print("NODE (process %i) IS SENDING %s TOKEN", process_id, (receivedTokenColor == BLACK_COLOR ? "BLACK": "WHITE"));
		    	MPI_Send(&receivedTokenColor, 1, MPI_SHORT, (process_id + 1) % processors , MSG_TOKEN, MPI_COMM_WORLD);
		    }

		}

		if((isStackEmpty() || (counter % CONTROL_MESSAGE_FLAG) == 0) && breakProcess == 0) {

			/* Serve messages */
			do {
				isMessage = 0;

				MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
				if(flag == 1) {
					isMessage = 1;
					switch(status.MPI_TAG) {
						case MSG_EMERGENCY_END: {
							debug_print("MSG_EMERGENCY_END (process %i) RECEIVED", process_id);
							return;
						}
						case MSG_END: {
							debug_print("MSG_END (process %i) RECEIVED", process_id);
							/* END, shut down */
							breakProcess = 1;
						}
						break;
						case MSG_TOKEN: {
							debug_print("MSG_TOKEN (process %i) RECEIVED", process_id);

							MPI_Recv(&receivedTokenColor, 1, MPI_SHORT, MPI_ANY_SOURCE, MSG_TOKEN, MPI_COMM_WORLD, &status);

							if (process_id != 0) {
								if(color == BLACK_COLOR) {
									receivedTokenColor = BLACK_COLOR;
								}
								hasToken = 1;
							} else {
								if (receivedTokenColor == WHITE_COLOR && isStackEmpty()) {
									/* end process */
									for (i = 0; i < processors; i++) {
										MPI_Send(&processors, 1, MPI_SHORT, i, MSG_END, MPI_COMM_WORLD);
										breakProcess = 1;
									}
								} else {
									tokenSent = 0;
								}
							}
						}
						break;
						case MSG_INIT: {
							debug_print("MSG_INIT (process %i) RECEIVED", process_id);

							/* initializovat stav */

							MPI_Recv(initData, 5, MPI_SHORT, MPI_ANY_SOURCE, MSG_INIT, MPI_COMM_WORLD, &status);
							init(initData[0], initData[1], initData[2], initData[3], initData[4]);
							global_state = RUNNING;
						}
						break;
						case MSG_NEW_MIN_STEPS: {
							/* zmensit max a nastavit info o nejlepsim reseni */
							debug_print("MSG_NEW_MIN_STEPS (process %i) RECEIVED", process_id);

							MPI_Recv(&newMinSteps , 1, MPI_SHORT, MPI_ANY_SOURCE, MSG_NEW_MIN_STEPS, MPI_COMM_WORLD, &status);

							if(newMinSteps < minSteps) {
								minSteps = max = newMinSteps;
								processorWithSolution = status.MPI_SOURCE;
								freeInspectStack();
							}
						}
						break;
						case MSG_NO_WORK: {
							debug_print("MSG_NO_WORK (process %i) RECEIVED", process_id);

							askewForWork = 0;
							MPI_Recv(&askewForWork, 1, MPI_SHORT, MPI_ANY_SOURCE, MSG_NO_WORK, MPI_COMM_WORLD, &status);
						}
						break;
						case MSG_WORK: {

#ifdef STAR
							int mpi_count;
							MPI_Get_count(&status, MPI_SHORT, &mpi_count);
							short data[mpi_count];
							askewForWork = 0;
							debug_print("MSG_WORK (process %i) OF SIZE %i RECEIVED", process_id, mpi_count);
							MPI_Recv(&data, mpi_count, MPI_SHORT, MPI_ANY_SOURCE, MSG_WORK, MPI_COMM_WORLD, &status);
							deserializeStack(data, mpi_count);
#else
							short items[status._count];
							askewForWork = 0;
							debug_print("MSG_WORK (process %i) OF SIZE %i RECEIVED", process_id, status._count);
							MPI_Recv(&items, status._count, MPI_SHORT, MPI_ANY_SOURCE, MSG_WORK, MPI_COMM_WORLD, &status);
							deserializeStack(items, status._count);
#endif


						}
						break;
						case MSG_WANT_WORK: {
							short * data = NULL, dataSize, sorry = 0;

							MPI_Recv(&dev_null, 1, MPI_SHORT, MPI_ANY_SOURCE, MSG_WANT_WORK, MPI_COMM_WORLD, &status);

							if(askewForWork == 1) {
								sorry = 1;
								/* i have no work too */
							} else {
								data =	serializeStack(&dataSize);
								if(dataSize > 0) {
									debug_print("MSG_WANT_WORK (process %i) FROM process %i RECEIVED - DATA OF SIZE %i WILL BE SEND", process_id, status.MPI_SOURCE, dataSize);
									/* I can send some work */
									MPI_Send(data, dataSize, MPI_SHORT, status.MPI_SOURCE, MSG_WORK, MPI_COMM_WORLD);
									color = BLACK_COLOR;
									/* FREE?? */
								} else {
									sorry = 1;
								}
							}

							if(sorry == 1) {
								/* sorry */
								debug_print("MSG_WANT_WORK (process %i) FROM process %i RECEIVED - NO WORK SEND", process_id, status.MPI_SOURCE);
								short x = 0;
								MPI_Send(&x, 1, MPI_SHORT, status.MPI_SOURCE, MSG_NO_WORK, MPI_COMM_WORLD);
							}

							if(data != NULL) {
								free(data);
							}

						}
						break;
					}
			    }
			} while(isMessage == 1 && breakProcess == 0);
		}

		if(!isStackEmpty() && breakProcess == 0) {


			short step, iStart, jStart, prevMovedDisc, i, received, sent, moved = 0;
			Tower* _towers;

			_towers = deserializeState(top(&step, &iStart, &jStart, &prevMovedDisc, &received, &sent));
			/*if(process_id != 0) debug_print("QQQQQQQQQQQ (process %i) RECEIVED = %i, num= %i, i = %i, j = %i, sent = %i", process_id, received, stack->num, iStart, jStart, sent);*/

			if(received == 1) {
				emptyStackItems();
				continue;
			}

			if(sent == 1) {
				pop();
				continue;
			}

			if(step > max || loopDetected()) {
				/* not a perspective solution branch */
				freeTowers(_towers, &towersCount);
				pop();
				continue;
			}

			if (iStart == 0 && jStart == 0 && isDestTowerComplete(&_towers[destTower - 1], discsCount)) {
				debug_print("\n\nSOLUTION FOUND IN %i STEPS (process %i) \n\n", step, process_id);
				printState(_towers, towersCount);
				fflush(stdout);

				if (step < minSteps) {

					max = step;
					minSteps = step;
					processorWithSolution = process_id;

					inspectStack();
					if(step <= min) {
						for(i = 0; i < processors; i++) {
							if(i == process_id) continue;
							debug_print("SENDING BEST SOLUTION (process %i) -> END TO process %i", process_id, i);
							MPI_Send(&processors, 1, MPI_SHORT, i, MSG_END, MPI_COMM_WORLD);
						}
						breakProcess = 1;
					} else {
						/* not the greatest solution */
						for(i = 0; i < processors; i++) {
							if(i == process_id) continue;
							debug_print("SENDING NEW MIN (process %i) -> END TO process %i", process_id, i);
							MPI_Send(&minSteps, 1, MPI_SHORT, i, MSG_NEW_MIN_STEPS, MPI_COMM_WORLD);
						}
					}
				}
				freeTowers(_towers, &towersCount);
				pop();
				continue;
			}

			for(i = iStart; i < towersCount; i++) {
				short j;
				for(j = jStart; j < towersCount; j++) {
					int resultDisc;
					if(i == j) {
						continue;
					}

					resultDisc = move(&_towers[i],&_towers[j]);

					if(resultDisc > 0) {
						if(j+1 >= towersCount) {
							setState(i+1, 0);
						} else {
							setState(i, j+1);
						}
						if(moved == 0) {
							if(prevMovedDisc != resultDisc) {
								push(serializeState(_towers), step+1, 0, 0, resultDisc, 0, 0);
								moved++;
							}
						}
						break;
					}
					jStart = 0;
				}
				if(moved > 0) {
					break;
				}
			}
			freeTowers(_towers, &towersCount);
			if(moved == 0) {
				/* no possible move - item is dead */
				pop();
			}
		}
	}

	freeStack();
	printSolutionIfExists();
	return;

}

void describeMove(short* prevState, short* currentState, short* disc, short* sourceTower, short* destTower) {
	int i;
	for(i = 0; i < discsCount; i++) {
		if(prevState[i] != currentState[i]) {
			*disc = i+1;
			*sourceTower = prevState[i] + 1;
			*destTower = currentState[i] + 1;
			return;
		}
	}
	*disc = *sourceTower = *destTower = -1;
}

int compareStates(short* prevState, short* currentState) {
	int i;
	for(i = 0; i < discsCount; i++) {
		if(prevState[i] != currentState[i]) {
			return 0;
		}
	}
	return 1;
}

void inspectStack() {
	short* currentState;
	StackItem * tmp;
	ProcessItem * n;
	n = NULL;

	freeInspectStack();

	tmp = stack->top;
	currentState = stack->top->data;

	for(tmp = tmp->next; tmp; tmp = tmp->next) {
		n = (ProcessItem*) malloc(sizeof(* n));
		describeMove(tmp->data, currentState, &n->disc, &n->sourceTower, &n->destTower);
		currentState = tmp->data;
		n->next = solutionQueue.head;
		solutionQueue.head = n;
	}
}

void freeInspectStack() {
	ProcessItem * next;
	next = solutionQueue.head;
	while(next != NULL) {
		ProcessItem * tmp;
		tmp = next->next;
		free(next);
		next = tmp;
	}
	solutionQueue.head = NULL;
}

void printSolutionIfExists() {
	int i;
	short local_min = minSteps;
	short local_processor = processorWithSolution;
	MPI_Status status;

	debug_print("PRINTING SOLUTION (process %i)", process_id);

	if (process_id != 0) {
		initData[0] = minSteps;
		initData[1] = processorWithSolution;

		debug_print("SENDING SOLUTION (process %i) TO MASTER", process_id);
		MPI_Send(initData, 5, MPI_SHORT, 0, MSG_SOLUTION_INFO, MPI_COMM_WORLD);

		MPI_Recv(&processorWithSolution, 1, MPI_SHORT, MPI_ANY_SOURCE, MSG_FINAL_PROCESSOR, MPI_COMM_WORLD, &status);

		debug_print("SOLUTION SYNC (process %i) RECEIVED", process_id);
	} else {
		for (i = 1; i < processors; i++) {
			MPI_Recv(initData, 5, MPI_SHORT, MPI_ANY_SOURCE, MSG_SOLUTION_INFO, MPI_COMM_WORLD, &status);
			debug_print("MASTER RECEIVED LOCAL SOLUTION FROM %i", status.MPI_SOURCE);

			if(initData[0] < local_min) {
				local_min = initData[0];
				local_processor = initData[1];
			}
		}

		minSteps = local_min;
		processorWithSolution = local_processor;

		for (i = 1; i < processors; i++) {
			debug_print("MASTER SEND GLOBAL SOLUTION SYNC TO process %i", i);
			MPI_Send(&processorWithSolution, 1, MPI_SHORT, i, MSG_FINAL_PROCESSOR, MPI_COMM_WORLD);
		}

	}

	if(minSteps <= max && process_id == processorWithSolution) {
		ProcessItem* pi;
		pi = solutionQueue.head;

		printf("\n\nDONE, SOLUTION FOUND (process %i), Steps: %i\n\n", process_id, minSteps);
		while(pi != NULL) {
			printProcessItem(pi);
			pi = pi->next;
		}
	} else if (minSteps > max && process_id == 0) {
		printf("\nERROR: No solution found\n");
	}

	freeInspectStack();
}

void init(short _size, short _discsCount, short _destTower, short _min, short _max) {
	towersCount = _size;
	discsCount = _discsCount;
	destTower = _destTower;
	min = _min;
	max = _max;
	max = 15;
	minSteps = max + 1;

	global_state = RUNNING;
}

/* for processor_id == 0  */
void process_master(int _process_id, int _processors, Tower *_towers, short _size, short _discsCount, short _destTower) {
	/*int i;*/

	/* MPI info settings */
	process_id = _process_id;
	processors = _processors;
	stack = initializeStack();

	towersCount = _size;
	discsCount = _discsCount;
	destTower = _destTower;
	min = minMoves(_towers, towersCount, discsCount, destTower);
	max = maxMoves(discsCount, towersCount);
	max = 15;
	minSteps = max + 1;
    solutionQueue.head = NULL;
    global_state = RUNNING;

    /* state settings */
    global_state = RUNNING;

    /* send info for others:
     *
     * * towersCount
     * * discCount
     * * destTower
     * * min
     * * max
     *
     **/
    /*
    initData[0] = towersCount;
    initData[1] = discsCount;
    initData[2] = destTower;
    initData[3] = min;
    initData[4] = max;

    for(i = 1; i < processors; i++) {
    	MPI_Send(initData, 5, MPI_SHORT, i, MSG_INIT, MPI_COMM_WORLD);
    }
     */
    push(serializeState(_towers), 0, 0, 0, -1, 0, 0);

	run();
}

/* other processors */
void process(int _process_id, int _processors) {
	process_id = _process_id;
	processors = _processors;
	stack = initializeStack();

	/* state settings */
	state = IDLE;
	global_state = INIT;

	run();
}

