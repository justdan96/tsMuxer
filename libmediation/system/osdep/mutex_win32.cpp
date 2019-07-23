
#include "../mutex.h"

#ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x400
#endif

#include <windows.h>

Mutex::Mutex()
:
	m_mtxImpl( new CRITICAL_SECTION() )
{
	InitializeCriticalSection( (CRITICAL_SECTION*)m_mtxImpl );
}

Mutex::~Mutex()
{
	DeleteCriticalSection( (CRITICAL_SECTION*)m_mtxImpl );
	delete (LPCRITICAL_SECTION)m_mtxImpl;
	m_mtxImpl = 0;
}

void Mutex::lock()
{
	if( !m_mtxImpl )
		return;
	EnterCriticalSection( (CRITICAL_SECTION*)m_mtxImpl );
}

void Mutex::unlock()
{
	if( !m_mtxImpl )
		return;
	LeaveCriticalSection( (CRITICAL_SECTION*)m_mtxImpl );
}

//#endif

Mutex::ScopedLock::ScopedLock( Mutex* const mtx )
:
	m_mtx( mtx )
{
	m_mtx->lock();
}

Mutex::ScopedLock::~ScopedLock()
{
	m_mtx->unlock();
}
