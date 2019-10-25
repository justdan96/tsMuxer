#include "../terminatablethread.h"

#include <windows.h>

#include <mutex>

Terminatable::~Terminatable() 
{ 
}


class ThreadImpl
{
public:
	ThreadImpl()
	:
		m_threadHandle( 0 )
	{
	}
	
	~ThreadImpl()
	{
		if( m_threadHandle )
        {
            TerminateThread( m_threadHandle, 0 );
            CloseHandle ( m_threadHandle );
        }
	}

	void join()
	{
		WaitForSingleObject( m_threadHandle, INFINITE );
	}

	HANDLE m_threadHandle;
};


void threadProc( TerminatableThread* const ptr )
{
	ptr->thread_main();
}

DWORD WINAPI thread_proc( LPVOID lpParameter )
{
	threadProc( static_cast<TerminatableThread*>(lpParameter) );
	return 0;
}

TerminatableThread::TerminatableThread()
:
	m_impl( new ThreadImpl() )
{
}

TerminatableThread::~TerminatableThread() 
{
	delete static_cast<ThreadImpl*>(m_impl);
}

void TerminatableThread::run( TerminatableThread* const thread )
{
	static_cast<ThreadImpl*>(thread->m_impl)->m_threadHandle = 
		CreateThread(
			NULL, 
			0, 
			thread_proc, 
			thread, 
			0, 
			0 );
}

void TerminatableThread::join()
{
	static_cast<ThreadImpl*>(m_impl)->join();
}
