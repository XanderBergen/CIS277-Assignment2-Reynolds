#ifndef STACK_H
#define STACK_H

#include <cstddef>
#include <stdexcept>

template <typename T>
class Stack
{
private:
    struct Node
    {
        T data;
        Node* next;

        Node(const T& value, Node* nextNode = nullptr)
            : data(value), next(nextNode)
        {
        }
    };

    Node* topNode;
    std::size_t count;

public:
    // Constructor
    Stack()
        : topNode(nullptr), count(0)
    {
    }

    // Destructor
    ~Stack()
    {
        while (!empty())
        {
            pop();
        }
    }

    // Prevent accidental copying of the linked structure.
    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    // Add an item to the top of the stack.
    void push(const T& value)
    {
        topNode = new Node(value, topNode);
        ++count;
    }

    // Remove and return the item at the top of the stack.
    T pop()
    {
        if (empty())
        {
            throw std::underflow_error("Cannot pop from an empty stack.");
        }

        Node* nodeToRemove = topNode;
        T value = nodeToRemove->data;

        topNode = topNode->next;

        delete nodeToRemove;
        --count;

        return value;
    }

    // Return a reference to the item at the top of the stack.
    T& top()
    {
        if (empty())
        {
            throw std::underflow_error("Cannot access the top of an empty stack.");
        }

        return topNode->data;
    }

    // Return true if the stack contains no items.
    bool empty() const
    {
        return topNode == nullptr;
    }

    // Return the number of items currently in the stack.
    std::size_t size() const
    {
        return count;
    }
};

#endif
