
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
	//! После создания объекта треда необходимо вызвать run(thread) до первого использования треда
	TerminatableThread();
	//! Деструктор дожидается окончания работы треда и уничтожает объект
	virtual ~TerminatableThread(); 

	//! Запуск объекта треда. Должна быть вызвана непосредственно после создания объекта.
	static void run( TerminatableThread* const );

	void join();
    


protected:
	//! Главная функция треда. Должна быть реализована в потомках.
	virtual void thread_main() = 0;

private:
	TerminatableThread( const TerminatableThread& );
	TerminatableThread& operator=( const TerminatableThread& );

	friend void threadProc( TerminatableThread* const ptr );
	void* const m_impl;
};

#endif //_TERMINATABLE_THREAD_H
