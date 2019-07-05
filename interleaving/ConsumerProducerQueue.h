#ifndef __CONSUMERPRODUCERQUEUE_H__
#define __CONSUMERPRODUCERQUEUE_H__

#include <queue>
#include <mutex>
#include <condition_variable>

/*
 * Some references in order
 *
 * Some code I wrote a long time before C++ 11 to do consumer producer buffers, using 2 condition variables
 * https://github.com/mdaus/coda-oss/blob/master/modules/c%2B%2B/mt/include/mt/RequestQueue.h
 *
 * A great article explaining both 2 condition variable and 1 condition variable buffers
 * https://en.wikipedia.org/wiki/Monitor_%28synchronization%29#Condition_variables
 *
 * C++ 11 thread reference:
 * http://en.cppreference.com/w/cpp/thread
 */
template<typename T>
class ConsumerProducerQueue
{
    std::condition_variable cond;
    std::mutex mutex;
    std::queue<T> cpq;
    int maxSize;
public:
    ConsumerProducerQueue()
    { }

    void set_max(int max) {
        maxSize = max;
    }
    void add(T request)
    {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [this]()
        { return !isFull(); });
        cpq.push(request);
        lock.unlock();
        cond.notify_all();
    }

    void consume(T &request)
    {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [this]()
        { return !isEmpty(); });
        request = cpq.front();
        cpq.pop();
        lock.unlock();
        cond.notify_all();

    }

    bool isFull() const
    {
        return cpq.size() >= maxSize;
    }

    bool isEmpty() const
    {
        return cpq.size() == 0;
    }

    int length() const
    {
        return cpq.size();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (!isEmpty())
        {
            cpq.pop();
        }
        lock.unlock();
        cond.notify_all();
    }
};

#endif
