/*
 * processor.h
 *
 */

#ifndef PROCESSOR_H_
#define PROCESSOR_H_

int process_master(int _process_id, int _processors, Tower _towers[], int _size, int _discsCount, int _destTower);
int process(int _process_id, int _processors);

#endif /* PROCESSOR_H_ */
