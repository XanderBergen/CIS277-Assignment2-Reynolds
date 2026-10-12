#include "MemoryPool.h"

#include <cstdint>
#include <limits>
#include <stdexcept>

MemoryPool::MemoryPool(std::size_t blockSize, std::size_t blockCount)
    : memory(nullptr),
    blockSize_(blockSize),
    blockCount_(blockCount),
    freeBlocks(),
    allocated(nullptr)
{
    // A pool with zero blocks is simply empty.
    if (blockCount_ == 0)
    {
        return;
    }

    // Blocks need a positive size so each block has a distinct address.
    if (blockSize_ == 0)
    {
        throw std::invalid_argument("Block size must be greater than zero.");
    }

    // Prevent overflow when calculating the total pool capacity.
    if (blockCount_ > std::numeric_limits<std::size_t>::max() / blockSize_)
    {
        throw std::overflow_error("Memory pool size is too large.");
    }

    // Reserve one continuous region of memory for all blocks.
    memory = new unsigned char[capacity()];

    try
    {
        // All blocks begin as unallocated.
        allocated = new bool[blockCount_]();

        // Add the address of every block to the free-block stack.
        //
        // Pushing from Block 0 upward means the highest-numbered
        // block will be at the top of the stack first.
        for (std::size_t i = 0; i < blockCount_; ++i)
        {
            void* blockAddress = memory + (i * blockSize_);
            freeBlocks.push(blockAddress);
        }
    }
    catch (...)
    {
        delete[] allocated;
        delete[] memory;

        allocated = nullptr;
        memory = nullptr;

        throw;
    }
}

MemoryPool::~MemoryPool()
{
    delete[] allocated;
    delete[] memory;
}

void* MemoryPool::allocate()
{
    // No available blocks remain.
    if (freeBlocks.empty())
    {
        return nullptr;
    }

    // Remove the most recently available block.
    void* block = freeBlocks.pop();

    // Because this pointer came from our own free-block stack,
    // it is guaranteed to point to a valid block.
    unsigned char* blockPtr = static_cast<unsigned char*>(block);

    std::size_t index =
        static_cast<std::size_t>(blockPtr - memory) / blockSize_;

    allocated[index] = true;

    return block;
}

bool MemoryPool::deallocate(void* ptr)
{
    std::size_t index = 0;

    // Reject pointers that do not point to the beginning
    // of one of this pool's blocks.
    if (!getBlockIndex(ptr, index))
    {
        return false;
    }

    // If this block is already available, this is a
    // duplicate deallocation.
    if (!allocated[index])
    {
        return false;
    }

    // Mark the block as available again.
    allocated[index] = false;

    // Return it to the top of the free-block stack.
    freeBlocks.push(ptr);

    return true;
}

std::size_t MemoryPool::availableBlocks() const
{
    return freeBlocks.size();
}

std::size_t MemoryPool::allocatedBlocks() const
{
    return blockCount_ - freeBlocks.size();
}

std::size_t MemoryPool::blockSize() const
{
    return blockSize_;
}

std::size_t MemoryPool::capacity() const
{
    return blockSize_ * blockCount_;
}

bool MemoryPool::getBlockIndex(void* ptr, std::size_t& index) const
{
    if (ptr == nullptr ||
        memory == nullptr ||
        blockSize_ == 0 ||
        blockCount_ == 0)
    {
        return false;
    }

    // Convert the addresses to integer representations so we can
    // safely check whether ptr lies within our memory region.
    std::uintptr_t baseAddress =
        reinterpret_cast<std::uintptr_t>(memory);

    std::uintptr_t ptrAddress =
        reinterpret_cast<std::uintptr_t>(ptr);

    // Anything before the beginning of the pool is invalid.
    if (ptrAddress < baseAddress)
    {
        return false;
    }

    std::uintptr_t offset = ptrAddress - baseAddress;

    // Anything at or beyond the end of the pool is invalid.
    if (offset >= static_cast<std::uintptr_t>(capacity()))
    {
        return false;
    }

    // A valid pointer must point exactly to the beginning of a block.
    if (offset % blockSize_ != 0)
    {
        return false;
    }

    index = static_cast<std::size_t>(offset / blockSize_);

    return index < blockCount_;
}