/***********************************************************************
*	File: system/mutex_linux.cpp
*	Author: Andrey Kolesnikov
*	Date: 28 dec 2005
***********************************************************************/

/*
$Log: mutex_linux.cpp,v $
Revision 1.4  2007/06/22 14:20:19  andreyk
*** empty log message ***

Revision 1.3  2007/02/25 13:35:27  andreyk
Внесены небольшие изменения в интерфейс общих классов.

Revision 1.2  2006/09/21 10:56:10  andreyk
Исправлена ошибка в классе Mutex::ScopedLock: Если мютекс отпущен функцией unlock, то функция ScopedLock::~ScopedLock всё равно пыталась его отпустить, что приводило к исключению.

Revision 1.1  2006/04/19 17:00:03  mike
*** empty log message ***

Revision 1.3  2006/02/06 08:38:39  andreyk
В реализацию класса Mutex добавлена проверка на существавание системного объекта перед каждым его использованием.

Revision 1.2  2006/02/02 11:22:11  andreyk

Building mediation under linux

Revision 1.1  2005/12/28 14:34:34  andreyk

Building project under Linux

*/

#include "../mutex.h"

#include <pthread.h>


Mutex::ScopedLock::ScopedLock( Mutex* mtx )
:
	m_mtx( mtx )
{
	m_mtx->lock();
}

Mutex::ScopedLock::~ScopedLock()
{
	m_mtx->unlock();
}


Mutex::Mutex()
:
	m_mtxImpl( new pthread_mutex_t )
{
	pthread_mutex_init( reinterpret_cast<pthread_mutex_t*>(m_mtxImpl), NULL );
}

Mutex::~Mutex()
{
	pthread_mutex_destroy( reinterpret_cast<pthread_mutex_t*>(m_mtxImpl) );
	delete reinterpret_cast<pthread_mutex_t*>(m_mtxImpl);
	m_mtxImpl = 0;
}

void Mutex::lock()
{
	if( m_mtxImpl )
		pthread_mutex_lock( reinterpret_cast<pthread_mutex_t*>(m_mtxImpl) );
}


void Mutex::unlock()
{
	if( m_mtxImpl )
		pthread_mutex_unlock( reinterpret_cast<pthread_mutex_t*>(m_mtxImpl) );
}

