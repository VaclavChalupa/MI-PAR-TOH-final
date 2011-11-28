/*
 * processor.h
 *
 */

#ifndef PROCESSOR_H_
#define PROCESSOR_H_

int process_master(int _process_id, int _processors, Tower _towers[], short _size, short _discsCount, short _destTower);
int process(int _process_id, int _processors);

#endif /* PROCESSOR_H_ */
