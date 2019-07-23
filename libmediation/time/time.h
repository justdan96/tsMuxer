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
	// возвращает число милисекунд, прошедшее с момента запуска системы (под Windows) или
	// с неопределённого системного события в Linux.
	uint32_t  clockGetTime();

	// возвращает число микросекунд, прошедшее с момента неопределённого системного события.
	uint64_t  clockGetTimeEx(); 

};

#endif //__T_MTime_H
