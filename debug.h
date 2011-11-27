/*
 * debug.h
 *
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#ifdef DEBUG
#define USE_DEBUG 1
#else
#define USE_DEBUG 0
#endif

#ifdef FULL_DEBUG
#define USE_FULL_DEBUG 1
#else
#define USE_FULL_DEBUG 0
#endif

#include <stdio.h>

#define debug_print(fmt, ...) do { if (USE_DEBUG) { if(USE_FULL_DEBUG) { printf("%s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, __VA_ARGS__); } else { printf(fmt "\n", __VA_ARGS__); } fflush(stdout); } } while (0)



#endif /* DEBUG_H_ */
