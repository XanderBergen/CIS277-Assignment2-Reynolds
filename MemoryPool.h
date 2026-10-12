#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <cstddef>
#include "Stack.h"

class MemoryPool
{
private:
    unsigned char* memory;
    std::size_t blockSize_;
    std::size_t blockCount_;

    Stack<void*> freeBlocks;

    // Tracks whether each block is currently allocated.
    // true  = allocated
    // false = available
    bool* allocated;

    // Checks whether ptr refers to the beginning of a valid block
    // and determines that block's index.
    bool getBlockIndex(void* ptr, std::size_t& index) const;

public:
    MemoryPool(std::size_t blockSize, std::size_t blockCount);
    ~MemoryPool();

    void* allocate();
    bool deallocate(void* ptr);

    std::size_t availableBlocks() const;
    std::size_t allocatedBlocks() const;
    std::size_t blockSize() const;
    std::size_t capacity() const;

    // A MemoryPool owns its memory, so copying is disabled.
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
};

#endif

