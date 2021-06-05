/* 
    阻塞队列作为日志缓冲区，内部封装了生产者-消费者模型。

    生产者：向队列尾部插入日志信息的线程
    消费者：从队列头部取出日志信息并处理的线程

    因为涉及到多线程读写，使用互斥锁实现对队列的互斥访问，同时使用两个条件变量。

    插入和删除时，条件变量需要配合互斥锁。
    先上锁，然后在 while 循环内检查条件变量
        当插入一个日志时，唤醒一个消费者线程
        当处理一个日志时，唤醒一个生产者线程
*/
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>
#include <sys/time.h>

template<class T>
class BlockQueue {
public:
    explicit BlockQueue(size_t MaxCapacity = 1000);

    ~BlockQueue();

    void close();

    void clear();

    void flush();

    bool full();

    bool empty();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

private:
    std::queue<T> queue;

    size_t capacity_;

    std::mutex mtx;

    bool isClose;

    std::condition_variable condConsumer;

    std::condition_variable condProducer;
};


template<class T>
BlockQueue<T>::BlockQueue(size_t maxCapacity) :capacity_(maxCapacity) 
{
    assert(maxCapacity > 0);
    isClose = false;
}

template<class T>
BlockQueue<T>::~BlockQueue() 
{
    close();
}

template<class T>
void BlockQueue<T>::close() 
{
    {   
        std::lock_guard<std::mutex> locker(mtx);
        // queue不支持clear，但可以重新赋值
        queue = std::queue<T>();
        isClose = true;
    }
    condProducer.notify_all();
    condConsumer.notify_all();
}

template<class T>
void BlockQueue<T>::clear() 
{
    std::lock_guard<std::mutex> locker(mtx);
    queue.clear();
}

template<class T>
void BlockQueue<T>::flush() 
{
    condConsumer.notify_one();
}

template<class T>
bool BlockQueue<T>::full()
{
    std::lock_guard<std::mutex> locker(mtx);
    return queue.size() >= capacity_;
}

template<class T>
bool BlockQueue<T>::empty() 
{
    std::lock_guard<std::mutex> locker(mtx);
    return queue.empty();
}

template<class T>
size_t BlockQueue<T>::size() 
{
    std::lock_guard<std::mutex> locker(mtx);
    return queue.size();
}

template<class T>
size_t BlockQueue<T>::capacity() 
{
    std::lock_guard<std::mutex> locker(mtx);
    return capacity_;
}

template<class T>
T BlockQueue<T>::front() 
{
    std::lock_guard<std::mutex> locker(mtx);
    return queue.front();
}

template<class T>
T BlockQueue<T>::back() 
{
    std::lock_guard<std::mutex> locker(mtx);
    return queue.back();
}

template<class T>
void BlockQueue<T>::push(const T &item) 
{
    std::unique_lock<std::mutex> locker(mtx);
    while (queue.size() >= capacity_) 
    {
        condProducer.wait(locker);
    }
    queue.push(item);
    condConsumer.notify_one();
}

template<class T>
bool BlockQueue<T>::pop(T &item) 
{
    std::unique_lock<std::mutex> locker(mtx);
    while (queue.empty())
    {
        condConsumer.wait(locker);
        if (isClose) 
        {
            return false;
        }
    }
    item = queue.front();
    queue.pop();
    condProducer.notify_one();
    return true;
}

template<class T>
bool BlockQueue<T>::pop(T &item, int timeout) 
{
    std::unique_lock<std::mutex> locker(mtx);
    while (queue.empty())
    {
        // 判断等待时间是否超过 timeout
        if (condConsumer.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false;
        }
        if (isClose) 
        {
            return false;
        }
    }
    item = queue.front();
    queue.pop();
    condProducer.notify_one();
    return true;
}

#endif