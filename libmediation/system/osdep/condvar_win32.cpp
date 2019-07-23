#include "system/condvar.h"

#include <time.h>


enum 
{
	SIGNAL = 0,
	BROADCAST = 1,
	MAX_EVENTS = 2
};

class ConditionVariableImpl
{
public:
	ConditionVariableImpl()
	:
		m_waitersCount( 0 )
	{
		m_events[SIGNAL] = CreateEvent(
			NULL,  // no security
			FALSE, // auto-reset event
			FALSE, // non-signaled initially
			NULL ); // unnamed

		m_events[BROADCAST] = CreateEvent(
			NULL,  // no security
			TRUE,  // manual-reset
			FALSE, // non-signaled initially
			NULL ); // unnamed
	}

	~ConditionVariableImpl()
	{
		CloseHandle( m_events[SIGNAL] );
		CloseHandle( m_events[BROADCAST] );
	}

	void notify_one()
	{
		Mutex::ScopedLock sl( &m_waitersCountLock );

		if( m_waitersCount > 0 )
			SetEvent( m_events[SIGNAL] );
	}

	void notify_all()
	{
		Mutex::ScopedLock sl( &m_waitersCountLock );

		if( m_waitersCount > 0 )
			SetEvent( m_events[BROADCAST] );
	}

	void do_wait( Mutex* const mtx )
	{
		{
			Mutex::ScopedLock sl( &m_waitersCountLock );
			m_waitersCount++;
		}

		mtx->unlock();

		int result = WaitForMultipleObjects( 2, m_events, FALSE, INFINITE );

		{
			Mutex::ScopedLock sl( &m_waitersCountLock );

			m_waitersCount--;
			if(	(result == WAIT_OBJECT_0 + BROADCAST) && (m_waitersCount == 0) )
				ResetEvent( m_events[BROADCAST] );
		}

		mtx->lock();
	}


private:
	HANDLE m_events[MAX_EVENTS];
	unsigned int m_waitersCount;
	Mutex m_waitersCountLock;
};

ConditionVariable::ConditionVariable()
:
	m_impl( new ConditionVariableImpl() )
{
}

ConditionVariable::~ConditionVariable()
{
	delete ((ConditionVariableImpl*)m_impl);
}

void ConditionVariable::notify_all()
{
	((ConditionVariableImpl*)m_impl)->notify_all();
}

void ConditionVariable::notify_one() 
{
	((ConditionVariableImpl*)m_impl)->notify_one(); 
}

void ConditionVariable::wait( Mutex::ScopedLock* const lock )
{
    ((ConditionVariableImpl*)m_impl)->do_wait( lock->m_mtx );
}

