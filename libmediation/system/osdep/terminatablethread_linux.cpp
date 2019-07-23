/***********************************************************************
*	File: system/terminatablethread_linux.cpp
*	Author: Andrey Kolesnikov
*	Date: 28 dec 2005
***********************************************************************/

/*
$Log: terminatablethread_linux.cpp,v $
Revision 1.5  2006/09/14 19:11:21  andreyk

Corrected daemon-startup sequence.

Revision 1.4  2006/08/31 19:22:51  andreyk
*** empty log message ***

Revision 1.3  2006/08/30 13:00:09  andreyk
*** empty log message ***

Revision 1.2  2006/06/15 17:04:26  andreyk
Создано единое хранилище voip-сессий.

Revision 1.1  2006/04/19 17:00:03  mike
*** empty log message ***

Revision 1.2  2006/02/02 16:14:40  andreyk

Build project under linux

Revision 1.1  2005/12/28 14:34:34  andreyk

Building project under Linux

*/

#include "../terminatablethread.h"

extern "C" {
#include <pthread.h>
}

#include <iostream>
#include <fs/systemlog.h>
#include "errno.h"

using namespace std;

const static int PRIORITIES [ 6 ] = { 19, 10, 0, 10, -19, -20 };

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
			pthread_detach(m_threadHandle);
			pthread_cancel( m_threadHandle );
			m_threadHandle = 0;
		}
	}

	void join()
	{
		if( m_threadHandle )
			pthread_join( m_threadHandle, NULL );
	}

#ifndef MAC  
    bool setPriority ( int nPriority )
    {
        return pthread_setschedprio ( m_threadHandle, PRIORITIES [ nPriority ] ) == 0;
    }
#endif    

	pthread_t m_threadHandle;
};


void threadProc( TerminatableThread* const ptr )
{
	ptr->thread_main();
	//pthread_t handle = reinterpret_cast<ThreadImpl*>(ptr->m_impl)->m_threadHandle;
    //reinterpret_cast<ThreadImpl*>(ptr->m_impl)->m_threadHandle = 0;
	//pthread_detach(handle);
    pthread_exit ( NULL );
}
void* thread_proc( void* param )
{
	threadProc( reinterpret_cast<TerminatableThread*>(param) );
	return NULL;
}

TerminatableThread::TerminatableThread()
:
	m_impl( new ThreadImpl() )
{
}

TerminatableThread::~TerminatableThread() 
{
	delete reinterpret_cast<ThreadImpl*>(m_impl);
}

void TerminatableThread::run( TerminatableThread* const thread )
{
    int err = 0;
    size_t stackSize = 0;

    pthread_attr_t attr;
    
    if ( ( err = pthread_attr_init ( &attr ) ) == 0 )
    {
        //LTRACE(LT_DEBUG, 0, "TerminatableThread::run: pthread_attr_init success" )    
        //cout << "TerminatableThread::run: pthread_attr_init success" << endl;
    
        if ( ( err = pthread_attr_getstacksize ( &attr, &stackSize ) ) == 0 )
        {
            //LTRACE(LT_DEBUG, 0, "TerminatableThread::run: pthread_attr_getstacksize success, stackSize=" << stackSize )
            //cout << "TerminatableThread::run: pthread_attr_getstacksize success, stackSize=" << stackSize << endl;
            
            stackSize = 1024 * 1024;
            
            if ( ( err = pthread_attr_setstacksize ( &attr, stackSize ) ) == 0 )
            {
                //cout << "TerminatableThread::run: pthread_attr_setstacksize success" << endl;
                
                if ( ( err = pthread_create ( &reinterpret_cast < ThreadImpl* >( thread->m_impl )->m_threadHandle, &attr, thread_proc, thread ) ) == 0 )
                {
                    //cout << "TerminatableThread::run: pthread_create success" << endl;
                    
                }
                else {
                    //cout << "TerminatableThread::run: pthread_create fail, errno=" << err << endl;
		}
            }
            else {
                //cout << "TerminatableThread::run: pthread_attr_setstacksize fail, errno=" << err << endl;
	    }
        }
        else {
            //cout << "TerminatableThread::run: pthread_attr_getstacksize fail, errno=" << err << endl;
	}
    }
    else {
        //cout << "TerminatableThread::run: pthread_attr_init fail, errno=" << err << endl;
    }
}

void TerminatableThread::join()
{
	reinterpret_cast<ThreadImpl*>(m_impl)->join();
}

Terminatable::~Terminatable() 
{ 
}


//---------------------------------------------------------------------------------------
