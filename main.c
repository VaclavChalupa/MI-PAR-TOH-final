/*
 * main.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tower.h"
#include "state_printer.h"
#include "analyser.h"
#include "processor.h"
#include "mpi.h"
#include "debug.h"

int process_id, processors;

int main(int argc, char *argv[]) {
	Tower *towers;
	short discsCount, towersCount, destTower;
	static const char filename[] = "enter.txt";
	static const char discDelimiter[] = ",";
	FILE *file;
	short i;

	/* for time measurement */
	double t1, t2;

	/* initialize MPI */
	MPI_Init(&argc, &argv);

	/* find out process id */
	MPI_Comm_rank(MPI_COMM_WORLD, &process_id);

	/* find out number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &processors);

	if(process_id == 0) {
		debug_print("Started for %i processors", processors);
	}

	debug_print("START process %i", process_id);

	if (process_id == 0) {
		file = fopen(filename, "r");
		if ( file != NULL ) {
			  char line [ 128 ]; /* max line size */
			  int i;
			  fgets(line, sizeof line, file);
			  sscanf(line, "%hi", &discsCount);
			  fgets(line, sizeof line, file);
			  sscanf(line, "%hi", &towersCount);
			  fgets(line, sizeof line, file);
			  sscanf(line, "%hi", &destTower);

			  printf("Towers of Hanoi: %i towers, %i discs, %i dest tower\n", towersCount, discsCount, destTower);
			  fflush(stdout);

			  towers = (Tower*) malloc (towersCount * sizeof(Tower));

			  for (i = 0; i < towersCount; i++) {
				  char towerLine [ 128 ];
				  Tower tower = {0, NULL};
				  char *disc;

				  tower.number = i+1;

				  if(fgets(towerLine, sizeof towerLine, file) == NULL) {
					  return 1; /* bad enter */
				  }

				  disc = strtok(towerLine, discDelimiter);
				  while (1) {
					  short discSize = 0;
					  if (disc == NULL) {
						  break;
					  }

					  sscanf(disc, "%hi", &discSize);

					  if (discSize == 0) {
						  break;
					  }

					  insertDics(discSize, &tower);
					  disc = strtok(NULL, discDelimiter);
				  }
				  towers[i] = tower;
			  }

			  fclose(file);

			  printState(towers, towersCount);
			  fflush(stdout);

			  debug_print("Enter loaded by process %i", process_id);

			  /* wait for late-comers and start measuring time */
			  MPI_Barrier(MPI_COMM_WORLD); /* cekam na spusteni vsech procesu */
		  	  t1=MPI_Wtime(); /* pocatecni cas */

			  /* vstup do procesu jako master */
			  process_master(process_id, processors, towers, towersCount, discsCount, destTower);
			  freeTowers(towers, &towersCount);

			  /* wait for others to finish and calculate the total time */
			  MPI_Barrier(MPI_COMM_WORLD); /* cekam na dokonceni vypoctu */
			  t2=MPI_Wtime(); /* koncovy cas */
			  printf ("Total time spent: %f.\n",t2-t1);
		} else {
			for(i = 1; i < processors; i++) {
				MPI_Send(&i, 0, MPI_INT, i, 500, MPI_COMM_WORLD);
			}
			perror ("\nERROR: enter.txt could not be opened");
			fflush(stdout);
		}

	} else {
		/* not the master */
		MPI_Barrier(MPI_COMM_WORLD); /* I've started */
		process(process_id, processors);
		MPI_Barrier(MPI_COMM_WORLD); /* I've finished */
	}

	debug_print("FINISH process %i", process_id);

	/* shut down MPI */
	MPI_Finalize();
	return 0;

}
