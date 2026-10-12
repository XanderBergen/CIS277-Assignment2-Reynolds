
#include "PacketQueue.h"

#include <cstring>

PacketQueue::PacketQueue(std::size_t blockSize, std::size_t blockCount)
    : pool(blockSize, blockCount), packetQueue(blockCount)
{
}

bool PacketQueue::receive(std::size_t id, const unsigned char* bytes,
    std::size_t length)
{
    // Reject invalid packets or a lack of space without changing any state.
    if (bytes == nullptr || length == 0 || length > pool.blockSize() ||
        packetQueue.full() || pool.availableBlocks() == 0)
    {
        return false;
    }

    // The MemoryPool owns the storage for the packet's bytes.
    void* block = pool.allocate();
    if (block == nullptr)
    {
        return false;
    }

    // Copy the exact byte count; memcpy also preserves 0x00 and 0xFF.
    std::memcpy(block, bytes, length);

    // Only the descriptor is queued, not another copy of the payload.
    PacketDescriptor descriptor{ id, static_cast<unsigned char*>(block), length };

    if (!packetQueue.enqueue(descriptor))
    {
        // If enqueue cannot succeed, give the block back to the pool.
        pool.deallocate(block);
        return false;
    }

    return true;
}

bool PacketQueue::processNext(std::size_t& id,
    std::vector<unsigned char>& bytes)
{
    PacketDescriptor descriptor{};

    // On underflow, neither output argument nor internal state changes.
    if (!packetQueue.front(descriptor))
    {
        return false;
    }

    // Prepare the output before removing the descriptor. If copying throws,
    // the packet is still waiting and its block has not been released.
    std::vector<unsigned char> packetBytes(
        descriptor.block, descriptor.block + descriptor.length);

    // Remove the oldest descriptor, then deliver its bytes and ID.
    packetQueue.dequeue(descriptor);
    bytes.swap(packetBytes);
    id = descriptor.id;

    // Release the memory only after the packet bytes have been copied out.
    pool.deallocate(descriptor.block);

    return true;
}

std::size_t PacketQueue::pending() const
{
    return packetQueue.size();
}

std::size_t PacketQueue::availableBlocks() const
{
    return pool.availableBlocks();
}
