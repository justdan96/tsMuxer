#include "terminatablethread.h"

#include <cassert>

TerminatableThread::~TerminatableThread() { join(); }

void TerminatableThread::run(TerminatableThread *const t)
{
    assert(t->m_thread.get_id() == std::thread::id() &&
           "TerminatableThread::run() called on an already running thread");
    t->m_thread = std::thread([t]() { t->thread_main(); });
}
