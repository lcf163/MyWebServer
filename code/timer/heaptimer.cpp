#include "heaptimer.h"

/* 
    数组模拟堆
    堆是一个完全二叉树，小根堆的每个节点的值都小于等于左右节点的值，根节点的值是全部数据的最小值。

    根节点记作 0 号节点，对于节点 x 来说：
        它的左子节点为 2x + 1，
        它的右子节点为 2x + 2，
        它的父节点为 (x - 1) / 2

    swap 操作：交换两个结点，同时更新哈希表内每个定时器的下标
    down 操作：该节点的值比左子节点或右子节点大，与左子节点、右子节点的最小值交换
    up 操作：该节点的值比父节点小，与父节点交换

    几个需求：
        增加定时器：查哈希表。如果是新定时器，插在最后，再up；如果不是新定时器，就需要调整定时器。
        调整定时器：更新定时后，再执行 down 和 up（实际上只会执行一个）
        删除定时器：与最后一个元素交换，删除末尾元素，然后再 down 和 up（删除任意位置结点，同样也只会执行一个）
*/
void HeapTimer::swapNode(size_t i, size_t j)
{
    size_t size = heap.size();
    assert(i >= 0 && i < size);
    assert(j >= 0 && j < size);
    std::swap(heap[i], heap[j]);
    ref[heap[i].id] = i;
    ref[heap[j].id] = j;
}

void HeapTimer::siftup(size_t i)
{
    assert(i >= 0 && i < heap.size());

    size_t j = (i - 1) / 2;
    while (j >= 0 && heap[i] < heap[j]) 
    {
        swapNode(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::siftdown(size_t i)
{
    size_t size = heap.size();
    assert(i >= 0 && i < size);
    size_t t = i * 2 + 1;

    while (t < size)
    {
        if (t + 1 < size && heap[t + 1] < heap[t]) t ++;
        if (heap[i] < heap[t]) break;
        swap(heap[i], heap[t]);
        i = t;
        t = i * 2 + 1;
    }
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb)
{
    assert(id >= 0);
    size_t i;
    if (!ref.count(id))
    {
        // 新节点：堆尾插入，向上调整
        i = heap.size();
        ref[id] = i;
        heap.push_back({id, Clock::now() + MS(timeout), cb});
        siftup(i);
    } 
    else 
    {
        // 已有结点：调整堆
        i = ref[id];
        heap[i].expires = Clock::now() + MS(timeout);
        heap[i].cb = cb;
        siftdown(i);
        siftup(i);
    }
}

void HeapTimer::del(size_t i)
{
    assert(!heap.empty() && i >= 0 && i < heap.size());
    // 将要删除的结点换到末尾，然后调整堆
    size_t n = heap.size() - 1;
    swapNode(i, n);

    ref.erase(heap.back().id);
    heap.pop_back();
    // 如果堆为空，不用调整
    if (!heap.empty())
    {
        siftdown(i);
        siftup(i);
    }
}

void HeapTimer::adjust(int id, int timeout)
{
    assert(!heap.empty() && ref.count(id));
    heap[ref[id]].expires = Clock::now() + MS(timeout);
    siftdown(ref[id]);
    siftup(ref[id]);
}

void HeapTimer::tick()
{
    if (heap.empty()) return;

    // 清除超时结点
    while (!heap.empty())
    {
        TimerNode node = heap.front();

        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0)
        {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop()
{
    assert(!heap.empty());
    del(0);
}

void HeapTimer::clear() 
{
    ref.clear();
    heap.clear();
}

int HeapTimer::getNextTick()
{
    // 处理堆顶计时器，若超时执行回调再删除
    tick();
    size_t res = -1;
    if (!heap.empty())
    {
        // 计算现在堆顶的超时时间，到期时先唤醒一次 epoll，判断是否有新事件
        res = std::chrono::duration_cast<MS>(heap.front().expires - Clock::now()).count();
        if (res < 0) { res = 0; }
    }
    return res;
}
