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

#include "state_printer.h";
#include "tower.h"
#include "stack_item.h"
#include "stack.h"
#include "process_item.h"
#include "analyser.h"
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include "debug.h"


/* MPI INFO */
int process_id;
int processors;
int state = IDLE;
int color = WHITE_COLOR;
int global_state;

/* solution info */
int towersCount;
int discsCount;
int min;
int max;
int minSteps;
int destTower;
int processorWithSolution = -1;
struct {
	ProcessItem *head;
} solutionQueue;

/* stack */
Stack * stack;

int initData[5];

static void describeMove(int* prevState, int* currentState, int* disc, int* sourceTower, int* destTower);
static int compareStates(int* prevState, int* currentState);
static void inspectStack();
static int*  serializeState(Tower* _towers);
static Tower* deserializeState(int* data);
static int loopDetected();
static void run();
static void freeInspectStack();
static void init(int _size, int _discsCount, int _destTower, int _min, int _max);
static void printSolutionIfExists();
static void deserializeStack(int* stackData, int itemsCount);
static int*  serializeStack(int* count);

int*  serializeState(Tower* _towers) {
	int * stack_item_data, i;
	stack_item_data = (int*) malloc(discsCount * sizeof(int));

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

Tower* deserializeState(int* data) {
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

void deserializeStack(int* stackData, int itemsCount) {

	int offset, bulk;
	bulk = discsCount + 6;

	for (offset = 0; offset < itemsCount; offset += bulk) {
		int* data;
		int step, i, j, movedDisc, received, sent;
		int disc;
		data = (int*) malloc(discsCount * sizeof(int));


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

	stack->num = 1;
}


int* serializeStack(int* count) {
	int cut;
	int* stackData;
	StackItem * tmp;
	StackItem* item;
	int offset = 0;
	int foundLevel = 0;
	int counter = 0;

	*count = 0;
	cut = stack->num - H;

	if(!isStackEmpty() && cut > 0) {
		/* found stack item in H-- */
		int level = stack->num;
		tmp = stack->top;
		for(tmp = tmp->next; tmp; tmp = tmp->next) {
			level--;
			/* send only perspective branche */
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

			stackData = (int*) malloc(counter * sizeof(int));
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
		}
		*count = counter;
		return stackData;
	}
	return stackData;
}

void run() {
	int workRequestProcessor = (process_id + 1) % processors;
	int askewForWork = 0;
	int counter = 0;
	int breakProcess = 0;
	int newMinSteps;
	int isMessage;

	int dev_null = 0;

	/* MPI */
	int flag;
	MPI_Status status;

	/* token processing */
	int hasToken = 0;
	int receivedTokenColor;
	int tokenSent = 0;

	int i;

	while(breakProcess == 0) {

		counter++;
		/* if root and no Work -> send token */
		if((isStackEmpty() && global_state == RUNNING) || (counter % (CONTROL_MESSAGE_FLAG)) == 0) {

			/* want work */
			if(isStackEmpty() && processors > 0 && askewForWork == 0 && global_state == RUNNING && processors > 1) {


				askewForWork = 1;
				MPI_Send(&workRequestProcessor, 1, MPI_INT, workRequestProcessor, MSG_WANT_WORK, MPI_COMM_WORLD);
				workRequestProcessor = (workRequestProcessor + 1) % processors;
				if(workRequestProcessor == process_id) {
					workRequestProcessor = (workRequestProcessor + 1) % processors;
				}

				debug_print("REQUESTING WORK (process %i) TO PROCESS %i", workRequestProcessor,process_id);
			}

			if(isStackEmpty() && process_id == 0 && tokenSent == 0) {
				if(processors > 1) {
					debug_print("MASTER (process %i) IS SENDING TOKEN", process_id);
					color = WHITE_COLOR;
					tokenSent = 1;
					MPI_Send(&color, 1, MPI_INT, 1, MSG_TOKEN, MPI_COMM_WORLD);
				} else {
					breakProcess = 1;
				}
		    } else if (isStackEmpty() && process_id != 0 && hasToken == 1) {
		    	color = WHITE_COLOR;
		    	hasToken = 0;
		    	debug_print("NODE (process %i) IS SENDING %s TOKEN", process_id, (receivedTokenColor == BLACK_COLOR ? "BLACK": "WHITE"));
		    	MPI_Send(&receivedTokenColor, 1, MPI_INT, (process_id + 1) % processors , MSG_TOKEN, MPI_COMM_WORLD);
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

							MPI_Recv(&receivedTokenColor, 1, MPI_INT, MPI_ANY_SOURCE, MSG_TOKEN, MPI_COMM_WORLD, &status);

							if (process_id != 0) {
								if(color == BLACK_COLOR) {
									receivedTokenColor = BLACK_COLOR;
								}
								hasToken = 1;
							} else {
								if (receivedTokenColor == WHITE_COLOR && isStackEmpty()) {
									/* end process */
									for (i = 0; i < processors; i++) {
										MPI_Send(&processors, 1, MPI_INT, i, MSG_END, MPI_COMM_WORLD);
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

							MPI_Recv(initData, 5, MPI_INT, MPI_ANY_SOURCE, MSG_INIT, MPI_COMM_WORLD, &status);
							init(initData[0], initData[1], initData[2], initData[3], initData[4]);
							global_state = RUNNING;
						}
						break;
						case MSG_NEW_MIN_STEPS: {
							/* zmensit max a nastavit info o nejlepsim reseni */
							debug_print("MSG_NEW_MIN_STEPS (process %i) RECEIVED", process_id);

							MPI_Recv(&newMinSteps , 1, MPI_INT, MPI_ANY_SOURCE, MSG_NEW_MIN_STEPS, MPI_COMM_WORLD, &status);

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
							MPI_Recv(&askewForWork, 1, MPI_INT, MPI_ANY_SOURCE, MSG_NO_WORK, MPI_COMM_WORLD, &status);
						}
						break;
						case MSG_WORK: {
							int items[status._count];

							askewForWork = 0;

							debug_print("MSG_WORK (process %i) OF SIZE %i RECEIVED", process_id, status._count);
							MPI_Recv(&items, status._count, MPI_INT, MPI_ANY_SOURCE, MSG_WORK, MPI_COMM_WORLD, &status);
							deserializeStack(items, status._count);

						}
						break;
						case MSG_WANT_WORK: {
							int * data, dataSize;


							MPI_Recv(&dev_null, 1, MPI_INT, MPI_ANY_SOURCE, MSG_WANT_WORK, MPI_COMM_WORLD, &status);
							data =	serializeStack(&dataSize);

							debug_print("MSG_WANT_WORK (process %i) FROM process %i RECEIVED - DATA OF SIZE %i WILL BE SEND", process_id, status.MPI_SOURCE, dataSize);

							if(dataSize > 0) {
								/* I can send some work */
								MPI_Send(data, dataSize, MPI_CHAR, status.MPI_SOURCE, MSG_WORK, MPI_COMM_WORLD);
								color = BLACK_COLOR;
								/* FREE?? */
							} else {
								/* sorry */
								int x = 0;
								MPI_Send(&x, 1, MPI_INT, status.MPI_SOURCE, MSG_NO_WORK, MPI_COMM_WORLD);
							}
						}
						break;
					}
			    }
			} while(isMessage == 1 && breakProcess == 0);
		}

		if(!isStackEmpty() && breakProcess == 0) {


			int step, iStart, jStart, prevMovedDisc, i, received, sent, moved = 0;
			Tower* _towers;

			_towers = deserializeState(top(&step, &iStart, &jStart, &prevMovedDisc, &received, &sent));

			if(step > max || loopDetected()) {
				/* not a perspective solution branch */
				freeTowers(_towers, &towersCount);
				pop();
				if(received == 1) {
					emptyStackItems();
				}
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
							MPI_Send(&processors, 1, MPI_INT, i, MSG_END, MPI_COMM_WORLD);
						}
						breakProcess = 1;
					} else {
						/* not the greatest solution */
						for(i = 0; i < processors; i++) {
							if(i == process_id) continue;
							debug_print("SENDING NEW MIN (process %i) -> END TO process %i", process_id, i);
							MPI_Send(&minSteps, 1, MPI_INT, i, MSG_NEW_MIN_STEPS, MPI_COMM_WORLD);
						}
					}
				}
				freeTowers(_towers, &towersCount);
				pop();

				if(received == 1) {
					/* END */
					emptyStackItems();
				}
				continue;
			}

			for(i = iStart; i < towersCount; i++) {
				int j;
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
				if(received == 1) {
					/* rest of stack is not my - END and request for work */
					emptyStackItems();
				}
			}
		}
	}

	freeStack();
	printSolutionIfExists();
	return;

}

void describeMove(int* prevState, int* currentState, int* disc, int* sourceTower, int* destTower) {
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

int compareStates(int* prevState, int* currentState) {
	int i;
	for(i = 0; i < discsCount; i++) {
		if(prevState[i] != currentState[i]) {
			return 0;
		}
	}
	return 1;
}

void inspectStack() {
	int* currentState;
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
	int local_min = minSteps;
	int local_processor = processorWithSolution;
	MPI_Status status;

	debug_print("PRINTING SOLUTION (process %i)", process_id);

	if (process_id != 0) {
		initData[0] = minSteps;
		initData[1] = processorWithSolution;

		debug_print("SENDING SOLUTION (process %i) TO MASTER", process_id);
		MPI_Send(initData, 5, MPI_INT, 0, MSG_SOLUTION_INFO, MPI_COMM_WORLD);

		MPI_Recv(&processorWithSolution, 1, MPI_INT, MPI_ANY_SOURCE, MSG_FINAL_PROCESSOR, MPI_COMM_WORLD, &status);

		debug_print("SOLUTION SYNC (process %i) RECEIVED", process_id);
	} else {
		for (i = 1; i < processors; i++) {
			MPI_Recv(initData, 5, MPI_INT, MPI_ANY_SOURCE, MSG_SOLUTION_INFO, MPI_COMM_WORLD, &status);
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
			MPI_Send(&processorWithSolution, 1, MPI_INT, i, MSG_FINAL_PROCESSOR, MPI_COMM_WORLD);
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
	} else if (process_id == 0) {
		printf("\nERROR: No solution found\n");
	}

	freeInspectStack();
}

void init(int _size, int _discsCount, int _destTower, int _min, int _max) {
	towersCount = _size;
	discsCount = _discsCount;
	destTower = _destTower;
	min = _min;
	max = _max;
	max = 15;
	minSteps = max + 1;

	state = ACTIVE;
	global_state = RUNNING;
}

/* for processor_id == 0  */
void process_master(int _process_id, int _processors, Tower *_towers, int _size, int _discsCount, int _destTower) {
	int i;

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
    state = ACTIVE;
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

    initData[0] = towersCount;
    initData[1] = discsCount;
    initData[2] = destTower;
    initData[3] = min;
    initData[4] = max;

    for(i = 1; i < processors; i++) {
    	MPI_Send(initData, 5, MPI_INT, i, MSG_INIT, MPI_COMM_WORLD);
    }

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

