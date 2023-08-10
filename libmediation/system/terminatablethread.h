#ifndef TERMINATABLE_THREAD_H
#define TERMINATABLE_THREAD_H

#include <thread>

class TerminatableThread
{
   public:
    //! After creating the thread object, it's necessary to call run(thread) until the thread's first usage.
    TerminatableThread() {}
    //! The destructor waits until the thread ends its work and destroys the object.
    virtual ~TerminatableThread();

    //! Launch the thread. Should be called immediately after creating the thread object.
    static void run(TerminatableThread*);

    void join()
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

   protected:
    //! Main thread function. Should be implemented in derived classes.
    virtual void thread_main() = 0;

   private:
    std::thread m_thread;
};

#endif  // TERMINATABLE_THREAD_H
