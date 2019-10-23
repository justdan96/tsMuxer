
#ifndef _TERMINATABLE_THREAD_H
#define _TERMINATABLE_THREAD_H

class  Terminatable
{
public:
	virtual ~Terminatable();

	virtual void terminate() = 0;
};

class  TerminatableThread: public Terminatable
{
public:
	//! After creating the thread object, it's necessary to call run(thread) until the thread's first usage.
	TerminatableThread();
	//! The destructor waits until the thread ends its work and destroys the object.
	virtual ~TerminatableThread(); 

	//! Launch the thread. Should be called immediately after creating the thread object.
	static void run( TerminatableThread* const );

	void join();
    


protected:
	//! Main thread function. Should be implemented in derived classes.
	virtual void thread_main() = 0;

private:
	TerminatableThread( const TerminatableThread& );
	TerminatableThread& operator=( const TerminatableThread& );

	friend void threadProc( TerminatableThread* const ptr );
	void* const m_impl;
};

#endif //_TERMINATABLE_THREAD_H
