
#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

#include <cstddef>
#include <vector>

#include "MemoryPool.h"
#include "Queue.h"

class PacketQueue
{
private:
    // A descriptor refers to bytes stored in the MemoryPool.
    // It does not store a second copy of the packet payload.
    struct PacketDescriptor
    {
        std::size_t id;
        unsigned char* block;
        std::size_t length;
    };

    // The pool owns the packet bytes; the queue tracks arrival order.
    MemoryPool pool;
    Queue<PacketDescriptor> packetQueue;

public:
    PacketQueue(std::size_t blockSize, std::size_t blockCount);

    // Store a packet in one pool block and enqueue its descriptor.
    // Return false if the input is invalid or no space is available.
    bool receive(std::size_t id, const unsigned char* bytes, std::size_t length);

    // Process the oldest packet, copy its bytes out, and release its block.
    // If no packet is pending, leave id and bytes unchanged and return false.
    bool processNext(std::size_t& id, std::vector<unsigned char>& bytes);

    std::size_t pending() const;
    std::size_t availableBlocks() const;
};

#endif

