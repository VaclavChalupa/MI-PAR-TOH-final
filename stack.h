/*
 * stack.h
 *
 */

#ifndef STACK_H_
#define STACK_H_

#include "stack_item.h"

typedef struct {
	StackItem *top;
	short num;
} Stack;

Stack * initializeStack();

void push(short* data, short step, short i, short j, short movedDisc, short received, short sent);

short* top(short* step, short* i, short* j, short* movedDisc, short* received, short* sent);

void pop();

void setState(short _i, short _j);

int isStackEmpty();

void freeStack();

void emptyStackItems();

void setReturning();

#endif /* STACK_H_ */
