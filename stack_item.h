/*
 * stack_item.h
 *
 */

#ifndef STACK_ITEM_H_
#define STACK_ITEM_H_

typedef struct StackItem {
	short* data;
	short step;
	short i;
	short j;
	short movedDisc;
	short received;
	short sent;
    struct StackItem *next;
} StackItem;

#endif /* STACK_ITEM_H_ */
