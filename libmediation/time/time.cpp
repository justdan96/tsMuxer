#include "time.h"

#include <time.h>
#include <ctime>


#ifdef WIN32
#include <sys/types.h> 
#include <sys/timeb.h>
#include <windows.h>
#endif

#ifdef LINUX
#include <sys/time.h>
#include <sys/timex.h>
     
#endif

#ifdef SOLARIS
//extern time_t timezone;
#include <sys/time.h>
#endif

#ifdef MAC
//extern time_t timezone;
#include <sys/time.h>
#endif

#include "../system/mutex.h"


namespace mtime
{

uint32_t clockGetTime() 
{
#ifdef WIN32
	return (uint32_t)GetTickCount();
#else 
	// POSIX
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (uint32_t) (tv.tv_sec * 1000ull + tv.tv_usec/1000);
#endif
}

uint64_t clockGetTimeEx()
{
#ifdef WIN32
	
	static uint32_t prevTics = 0;
	static uint64_t cycleCount = 0;
	static Mutex timeMutex;
	Mutex::ScopedLock lock(&timeMutex);
    uint32_t tics = (uint32_t) timeGetTime();
	if (tics < prevTics) 
		cycleCount+= 0x100000000ull;
	prevTics = tics;
	return ((uint64_t) tics + cycleCount) * 1000ull;
	

	/*
	static uint64_t freq = 0;
	if( freq == 0 )
	{
		LARGE_INTEGER timerFrequency;
		QueryPerformanceFrequency( &timerFrequency );
		freq = timerFrequency.QuadPart / 1000000ull;
	}

	LARGE_INTEGER t;
	QueryPerformanceCounter( &t );
	return (t.QuadPart+500000l) / freq;
	*/
#else 
	// POSIX
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000000ull + tv.tv_usec;
#endif
}

}
