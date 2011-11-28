/*
 * tower.h
 *
 */

#ifndef TOWER_H_
#define TOWER_H_


#include "disc.h"

typedef struct {
	short number;
	Disc *top;
} Tower;


int insertDics(short size, Tower *tower);

short move(Tower * source, Tower *dest);

int undoMove(Tower * source, Tower *dest);

int isDestTowerComplete(Tower *tower, short discCount);

void freeDiscs(Tower *tower);

void freeTowers(Tower* towers, short* towetsCount);

#endif /* TOWER_H_ */
