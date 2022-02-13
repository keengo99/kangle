/*
 * trace.h
 *
 *  Created on: 2010-5-24
 *      Author: keengo
 */

#ifndef TRACE_H_
#define TRACE_H_
#include "global.h"

typedef void * addr;
#define MAX_TRACEBACK_LEVELS 16
typedef addr TRACEBACK[MAX_TRACEBACK_LEVELS];
#ifndef _WIN32
#include <execinfo.h>
inline void generate_traceback(TRACEBACK tb) {
	backtrace(tb,MAX_TRACEBACK_LEVELS);
}
#endif
#endif /* TRACE_H_ */
