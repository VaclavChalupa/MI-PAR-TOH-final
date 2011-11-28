/*
 * tower.c
 *
 */

#include "tower.h"
#include "disc.h"
#include <stdlib.h>
#include <stdio.h>

static short processMove(Tower *source, Tower *dest);

static int verify(Tower *tower);

int insertDics(short size, Tower *tower) {
	Disc *disc = malloc(sizeof(* disc));
	disc->size = size;
	disc->next = NULL;

	if(tower->top != NULL) { /* NULL */
		disc->next = tower->top;
	}

	tower->top = disc;
	return 0;
}

short move(Tower *source, Tower *dest) {
	if(source->top != NULL && dest->top != NULL && dest->top->size < source->top->size) {
		return -2; /* dest has smaller disc*/
	}
	return processMove(source, dest);
}


int verify(Tower *tower) {
	Disc *disc;
	short prev = 0;
	disc = tower->top;

	while(disc != NULL) {
		if (prev > 0 && disc->size <= 0 ) {
			printf("ERROR in tower: %i", tower->number);
			exit(1);
		}
		prev = disc->size;
		disc = disc->next;
	}
	return 0;
}

int undoMove(Tower *source, Tower *dest) {
	verify(source);
	verify(dest);
	return processMove(dest, source);
}

int isDestTowerComplete(Tower *tower, short discCount) {
	Disc *disc;
	short index = 1;
	disc = tower->top;
	while(disc != NULL) {
		if (disc->size != index++) {
			return 0;
		}
		disc = disc->next;
	}
	return index == discCount+1 ? 1 : 0;
}

short processMove(Tower *source, Tower *dest) {
	Disc* tmp;

	if(source->top == NULL) {
		return -1; /* source has no disc */
	}

	tmp = source->top;
	source->top = source->top->next;
	tmp->next = dest->top;
	dest->top = tmp;

	return dest->top->size;
}

void freeDiscs(Tower *tower) {
	Disc *disc;
	disc = tower->top;
	while(disc != NULL) {
		Disc* tmp = disc->next;
		free(disc);
		disc = tmp;
	}
	tower->top = NULL;
}

void freeTowers(Tower* towers, short* towersCount) {
	int i;
	for(i = 0; i < *towersCount; i++) {
		freeDiscs(&towers[i]);
	}
	free(towers);
}

