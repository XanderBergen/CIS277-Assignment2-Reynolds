# CIS-277 Assignment 2: Network Packet Arrival Queue
## Student
Alexander Reynolds
## Design
This project is a Queue ADT using a fixed-capacity circular array that processes packets in FIFO order. frontIndex is used to track the oldest item in the queue, and count is used to track the number of items in the queue. I used the modulo operator (%) to allow indices to wrap without shifting elements, keeping the queue operations O(1). The PacketQueue class uses this queue to manage packet descriptors, while MemoryPool from Assignment 1 stores the actual packet bytes. The MemoryPool uses a Stack to reuse available blocks in LIFO order, while the Queue guarantees that packets are processed in their arrival order. Each packet's bytes are copied before its memory block is returned to the pool.
## How to Compile
Compile with a C++23 compiler using the following command:
```g++ -std=c++23 -Wall -Wextra -pedantic main.cpp PacketQueue.cpp MemoryPool.cpp -o packet_queue```
## How to Run
After compiling, run the program with ./packet_queue, or packet_queue.exe
## Tests and Results
I tested the Queue, MemoryPool, and PacketQueue using the tests in main.cpp.
- FIFO - Packets processed in arrival order - Passed
- Circular wraparound - Queue reuses empty slots correctly - Passed
- Queue overflow/underflow - Return false without changing state - Passed
- Front operation - Return oldest item without removing it - Passed
- Binary data - Preserve 0x00 and 0xFF - Passed
- Packet lengths - Accepts packets of 1 byte and exactly blockSize - Passed
- Oversized packets - Rejects packets larger than blockSize without changing state - Passed
- MemoryPool reuse - Released blocks become available again - Passed
- Empty processNext - Returns false without changing output arguments - Passed
- Invalid packet input - Rejects null pointers and 0-length packets without changing state - Passed
- Stress test - 80,000 operations without errors - Passed
## Analysis Questions
1. Why must packet service be FIFO even though free-block reuse is LIFO? Even though MemoryPool uses LIFO to efficiently reuse recently released memory blocks, the packet service must be FIFO so that all packets are processed in their arrival order, preventing older packets from being delayed by new ones.
2. How does your circular array wrap without shifting all waiting packets? The circular array uses modulo operator (%) to wrap indices around after they reach the end of the array. When removing an item, frontIndex is moved forward, and the slots for the new items to be placed in are determined by using (frontIndex + count) % items.size(). This lets the queue wrap without shifting any waiting packets.
3. What happens at queue overflow and underflow, and why must those attempts leave state unchanged? Queue overflow is when you try to add an item to a full queue, and underflow is when you try to remove an item from an empty queue. Both of these operations return false, and don't change the queue state. This prevents any data being lost, and keeps the queue consistent. 
4. What are the time complexities of queue operations and of receive/processNext when the packet has length bytes? Queue operations, such as enqueue, dequeue, and front take O(1) time, as no elements need to be shifted. In the case of receive and processNext, they take O(length) time, as they must copy the packet's bytes  into or out of MemoryPool. This means that queue operations will remain the same with increasing packet length, while receive and processNext will increase linearly in time as length increases. 
5. Why must processNext copy bytes out before returning the block to the pool? processNext must copy the packet's bytes before returning the block to the pool to prevent data loss. MemoryPool could use that block for another packet, and if the block is released first, its data could be overwritten before it is used. Copying the bytes guarantees that the packet's data is preserved before reusing the memory.
## AI Use
I used AI to help write my code, and test that it is all valid. I reviewed all of the AI-generated code and tested it to verify it works correctly, and fulfills the assignment specifications. 
