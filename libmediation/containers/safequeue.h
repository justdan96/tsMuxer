
#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class SafeQueue
{
   public:
    typedef typename std::queue<T>::size_type size_type;

    SafeQueue(const size_type& maxSize) : m_maxSize(maxSize) {}

    virtual ~SafeQueue() {}

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(m_mtx);

        return m_queue.empty();
    }

    size_type size() const
    {
        std::lock_guard<std::mutex> lk(m_mtx);

        return m_queue.size();
    }

    bool push(const T& val)
    {
        std::lock_guard<std::mutex> lk(m_mtx);

        if (m_queue.size() >= m_maxSize)
            return false;

        m_queue.push(val);
        return true;
    }

    T pop()
    {
        std::lock_guard<std::mutex> lk(m_mtx);

        T val = m_queue.front();
        m_queue.pop();

        return val;
    }

   private:
    mutable std::mutex m_mtx;
    std::queue<T> m_queue;
    const size_type m_maxSize;

    SafeQueue(const SafeQueue<T>&);
    SafeQueue& operator=(const SafeQueue<T>&);
};

template <typename T>

class SafeQueueWithNotification : public SafeQueue<T>
{
   public:
    SafeQueueWithNotification(const uint32_t maxSize, std::mutex& mtx, std::condition_variable& cond)
        : SafeQueue<T>(maxSize), m_mtx(mtx), m_cond(cond)
    {
    }

    bool push(const T& val)
    {
        std::lock_guard<std::mutex> lk(m_mtx);

        if (!SafeQueue<T>::push(val))
            return false;

        m_cond.notify_one();

        return true;
    }

   private:
    std::mutex& m_mtx;
    std::condition_variable& m_cond;

    SafeQueueWithNotification(const SafeQueueWithNotification<T>&);
    SafeQueueWithNotification& operator=(const SafeQueueWithNotification<T>&);
};

template <typename T>
class WaitableSafeQueue : public SafeQueueWithNotification<T>
{
   public:
    WaitableSafeQueue(const uint32_t maxSize) : SafeQueueWithNotification<T>(maxSize, m_mtx, m_cond) {}

    ~WaitableSafeQueue() override {}

    T pop()
    {
        std::unique_lock<std::mutex> lk(m_mtx);

        while (SafeQueue<T>::empty())
        {
            m_cond.wait(lk);
        }

        T val = SafeQueue<T>::pop();

        return val;
    }

   private:
    std::mutex m_mtx;
    std::condition_variable m_cond;

    WaitableSafeQueue(const WaitableSafeQueue<T>&);
    WaitableSafeQueue& operator=(const WaitableSafeQueue<T>&);
};

#endif  //_SAFE_QUEUE_H
