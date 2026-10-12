
#ifndef QUEUE_H
#define QUEUE_H

#include <cstddef>
#include <stdexcept>
#include <vector>

template <typename T>
class Queue
{
private:
    std::vector<T> items;
    std::size_t frontIndex;
    std::size_t count;

public:
    // Create a queue with a fixed, positive capacity.
    explicit Queue(std::size_t capacity)
        : items(capacity), frontIndex(0), count(0)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("Queue capacity must be greater than zero.");
        }
    }

    // Add an item at the back of the queue.
    // A full queue is left unchanged.
    bool enqueue(const T& value)
    {
        if (full())
        {
            return false;
        }

        std::size_t backIndex = (frontIndex + count) % items.size();
        items[backIndex] = value;
        ++count;

        return true;
    }

    // Remove the oldest item and copy it to value.
    // An empty queue leaves value unchanged.
    bool dequeue(T& value)
    {
        if (empty())
        {
            return false;
        }

        value = items[frontIndex];
        frontIndex = (frontIndex + 1) % items.size();
        --count;

        return true;
    }

    // Copy the oldest item to value without removing it.
    // An empty queue leaves value unchanged.
    bool front(T& value) const
    {
        if (empty())
        {
            return false;
        }

        value = items[frontIndex];
        return true;
    }

    bool empty() const
    {
        return count == 0;
    }

    bool full() const
    {
        return count == items.size();
    }

    std::size_t size() const
    {
        return count;
    }

    std::size_t capacity() const
    {
        return items.size();
    }
};

#endif

