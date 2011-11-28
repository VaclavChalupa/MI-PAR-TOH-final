/*
 * process_item.h
 *
 */

#ifndef PROCESS_ITEM_H_
#define PROCESS_ITEM_H_

typedef struct ProcessItem {
	short disc;
	short sourceTower;
	short destTower;
    struct ProcessItem *next;
} ProcessItem;

void printProcessItem(ProcessItem* processItem);

#endif /* PROCESS_ITEM_H_ */
