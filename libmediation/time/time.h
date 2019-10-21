#ifndef __T_MTime_H
#define __T_MTime_H

#include <string>

#include "types/types.h"


namespace mtime
{
	class TimeInterval;
	class DayTimeInterval;
};

namespace mtime
{

	// returns the number of milliseconds which have passed since the system started (under Windows) or
	// since an unspecified point in time under Linux.
	uint32_t  clockGetTime();

	// returns the number of microseconds which have passed since an unspecified point in time.
	uint64_t  clockGetTimeEx(); 

};

#endif //__T_MTime_H
