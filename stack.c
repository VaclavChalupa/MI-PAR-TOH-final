/*
 * stack.c
 *
 */

#include "stack.h"
#include <stdio.h>
#include <stdlib.h>

Stack * stack;
int x = 0;
static void pushItem(StackItem* stackItem, int step);

Stack * initializeStack() {
	stack = malloc(sizeof(* stack));
	stack->top = NULL;
	stack->num = 0;
	return stack;
}

void freeStack() {
	StackItem* stackItem = stack->top;
	while(stackItem != NULL) {
		StackItem* tmp = stackItem->next;
		free(stackItem->data);
		free(stackItem);
		stackItem = tmp;
	}
	stack->top = NULL;
	stack->num = 0;
	free(stack);
}

void emptyStackItems() {
	StackItem* stackItem = stack->top;
	while(stackItem != NULL) {
		StackItem* tmp = stackItem->next;
		free(stackItem->data);
		free(stackItem);
		stackItem = tmp;
	}
	stack->num = 0;
	stack->top = NULL;
}

void pushItem(StackItem* stackItem, int step) {
	stackItem->step = step;
	stackItem->next = stack->top;
	stack->top = stackItem;
	stack->num++;
}

void push(int* data, int step, int i, int j, int movedDisc, int received, int sent) {
	/*printf("\nPUSH\n");*/
	StackItem* stackItem;
	stackItem = (StackItem*) malloc(sizeof(* stackItem));

	stackItem->data = data;
	stackItem->next = NULL;
	stackItem->i = i;
	stackItem->j = j;
	stackItem->received = received;
	stackItem->sent = sent;
	stackItem->movedDisc = movedDisc;
	pushItem(stackItem, step);
}

void setState(int _i, int _j) {
	if(!isStackEmpty()) {
		stack->top->i = _i;
		stack->top->j = _j;
	}
}

int* top(int* step, int* i, int* j, int* movedDisc, int* received, int* sent) {
	if(isStackEmpty()) {
		perror("ERROR: stack is empty for top");
		return NULL;
	}
	*step = stack->top->step;
	*i = stack->top->i;
	*j = stack->top->j;
	*movedDisc = stack->top->movedDisc;
	*received = stack->top->received;
	*sent = stack->top->sent;
	return stack->top->data;
}

void pop() {
	StackItem* tmp;
	if(isStackEmpty()) {
		perror("ERROR: stack is empty for pop");
		return;
	}

	stack->num--;
	tmp = stack->top;
	stack->top = stack->top->next;
	free(tmp->data);
	free(tmp);
}

int isStackEmpty() {
	if(stack->top != NULL) {
		return 0;
	}
	return 1;
}
