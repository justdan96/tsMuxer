#ifndef IPSOFT_MUTEX_H
#define IPSOFT_MUTEX_H

class  Mutex
{
	friend class ScopedLock;
public:
	//!Этот класс должен использоваться только в одном потоке
	class  ScopedLock
	{
		friend class ConditionVariable;
	public:
		ScopedLock( Mutex* const );
		~ScopedLock();

	private:
		friend class ConditionVariableImpl;

		ScopedLock( const ScopedLock& );
		ScopedLock& operator=( const ScopedLock& );

		Mutex* const m_mtx;
	};

	Mutex();
	~Mutex();

	void lock();
	void unlock();

private:
	friend class ConditionVariableImpl;

	Mutex( const Mutex& );
	Mutex& operator=( const Mutex& );

	void* m_mtxImpl;
};

#endif// IPSOFT_MUTEX_H
