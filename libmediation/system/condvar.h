#ifndef IPSOFT_COND_VAR_H
#define IPSOFT_COND_VAR_H

#include "system/mutex.h"
#include "time/time.h"


class  ConditionVariable
{
public:
	ConditionVariable();
	~ConditionVariable();

    void notify_one();
    void notify_all();

    void wait( Mutex::ScopedLock* const lock );
private:
    void* const m_impl;

	ConditionVariable( const ConditionVariable& );
	ConditionVariable& operator=( const ConditionVariable& );

};

#endif// IPSOFT_COND_VAR_H
