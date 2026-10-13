// Public CIS-277 Assignment 2 checks. No student implementation is supplied.
#if __cplusplus < 202100L
#error "Assignment 2 requires a C++23-capable compiler and C++23 mode"
#endif

#include "Queue.h"
#include "PacketQueue.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T> struct QueueContract {
    using Q = Queue<T>;
    static_assert(std::is_constructible_v<Q, std::size_t>, "Queue must accept a capacity");
    static_assert(std::is_same_v<decltype(static_cast<bool (Q::*)(const T&)>(&Q::enqueue)), bool (Q::*)(const T&)>, "enqueue signature");
    static_assert(std::is_same_v<decltype(static_cast<bool (Q::*)(T&)>(&Q::dequeue)), bool (Q::*)(T&)>, "dequeue signature");
    static_assert(std::is_same_v<decltype(static_cast<bool (Q::*)(T&) const>(&Q::front)), bool (Q::*)(T&) const>, "front signature");
    static_assert(std::is_same_v<decltype(static_cast<bool (Q::*)() const>(&Q::empty)), bool (Q::*)() const>, "empty signature");
    static_assert(std::is_same_v<decltype(static_cast<bool (Q::*)() const>(&Q::full)), bool (Q::*)() const>, "full signature");
    static_assert(std::is_same_v<decltype(static_cast<std::size_t (Q::*)() const>(&Q::size)), std::size_t (Q::*)() const>, "size signature");
    static_assert(std::is_same_v<decltype(static_cast<std::size_t (Q::*)() const>(&Q::capacity)), std::size_t (Q::*)() const>, "capacity signature");
};
static_assert(sizeof(QueueContract<int>) > 0);
static_assert(sizeof(QueueContract<std::string>) > 0);
static_assert(std::is_constructible_v<PacketQueue, std::size_t, std::size_t>, "PacketQueue constructor signature");
static_assert(std::is_same_v<decltype(static_cast<bool (PacketQueue::*)(std::size_t, const unsigned char*, std::size_t)>(&PacketQueue::receive)), bool (PacketQueue::*)(std::size_t, const unsigned char*, std::size_t)>, "receive signature");
static_assert(std::is_same_v<decltype(static_cast<bool (PacketQueue::*)(std::size_t&, std::vector<unsigned char>&)>(&PacketQueue::processNext)), bool (PacketQueue::*)(std::size_t&, std::vector<unsigned char>&)>, "processNext signature");
static_assert(std::is_same_v<decltype(static_cast<std::size_t (PacketQueue::*)() const>(&PacketQueue::pending)), std::size_t (PacketQueue::*)() const>, "pending signature");
static_assert(std::is_same_v<decltype(static_cast<std::size_t (PacketQueue::*)() const>(&PacketQueue::availableBlocks)), std::size_t (PacketQueue::*)() const>, "availableBlocks signature");

void require(bool condition, const char* label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

void test_queue() {
    Queue<int> q(3);
    int value = 777;
    require(q.capacity() == 3 && q.size() == 0 && q.empty() && !q.full(), "initial queue state");
    require(!q.front(value) && value == 777, "front on empty preserves output");
    require(!q.dequeue(value) && value == 777, "dequeue on empty preserves output");
    require(q.enqueue(10) && q.enqueue(20) && q.enqueue(30), "enqueue to capacity");
    require(q.full() && q.size() == 3 && !q.enqueue(40) && q.size() == 3, "overflow leaves state unchanged");
    require(q.front(value) && value == 10 && q.size() == 3, "front does not remove");
    require(q.dequeue(value) && value == 10, "first out");
    require(q.enqueue(40) && q.full(), "wraparound insertion");
    require(q.dequeue(value) && value == 20, "second out");
    require(q.dequeue(value) && value == 30, "third out");
    require(q.dequeue(value) && value == 40, "wrapped item out");
    require(q.empty() && q.size() == 0, "empty after drain");
    require(q.enqueue(50) && q.dequeue(value) && value == 50, "reuse after drain");

    Queue<std::string> words(2);
    std::string word = "untouched";
    require(words.enqueue("alpha") && words.enqueue("beta"), "generic string enqueue");
    require(words.dequeue(word) && word == "alpha", "generic string dequeue");
    require(words.enqueue("gamma"), "generic string wraparound");
    require(words.front(word) && word == "beta", "generic string front");
    require(words.dequeue(word) && word == "beta", "generic string FIFO 1");
    require(words.dequeue(word) && word == "gamma", "generic string FIFO 2");
}

void test_packet_queue() {
    PacketQueue queue(4, 3);
    const unsigned char first[] = {0x00, 0xff, 0x12, 0x34};
    const unsigned char second[] = {0x55};
    const unsigned char third[] = {0x00, 0x80};
    const unsigned char fourth[] = {0xaa, 0xbb, 0xcc};
    std::size_t id = 999;
    std::vector<unsigned char> bytes = {0x42};
    require(queue.pending() == 0 && queue.availableBlocks() == 3, "initial packet state");
    require(!queue.processNext(id, bytes) && id == 999 && bytes == std::vector<unsigned char>{0x42}, "empty process preserves outputs");
    require(!queue.receive(99, nullptr, 1), "null input rejected");
    require(!queue.receive(99, first, 0), "zero length rejected");
    require(!queue.receive(99, first, 5), "oversize rejected");
    require(queue.pending() == 0 && queue.availableBlocks() == 3, "invalid input preserves state");

    require(queue.receive(1, first, 4), "first packet received");
    require(queue.receive(2, second, 1), "second packet received");
    require(queue.receive(3, third, 2), "third packet received");
    require(queue.pending() == 3 && queue.availableBlocks() == 0, "queue and pool full");
    require(!queue.receive(4, fourth, 3), "full receive rejected");
    require(queue.pending() == 3 && queue.availableBlocks() == 0, "full rejection preserves state");
    require(queue.processNext(id, bytes) && id == 1 && bytes == std::vector<unsigned char>(first, first + 4), "first packet data and FIFO");
    require(queue.pending() == 2 && queue.availableBlocks() == 1, "block released after processing");
    require(queue.receive(4, fourth, 3), "receive after block reuse");
    require(queue.pending() == 3 && queue.availableBlocks() == 0, "count after reuse");
    require(queue.processNext(id, bytes) && id == 2 && bytes == std::vector<unsigned char>(second, second + 1), "second packet remains next");
    require(queue.processNext(id, bytes) && id == 3 && bytes == std::vector<unsigned char>(third, third + 2), "third packet remains next");
    require(queue.processNext(id, bytes) && id == 4 && bytes == std::vector<unsigned char>(fourth, fourth + 3), "fourth packet after wraparound");
    require(queue.pending() == 0 && queue.availableBlocks() == 3, "all blocks released");
    id = 888;
    bytes = {0x77};
    require(!queue.processNext(id, bytes) && id == 888 && bytes == std::vector<unsigned char>{0x77}, "drained process preserves outputs");

    PacketQueue other(1, 1);
    const unsigned char one[] = {0xff};
    require(other.receive(7, one, 1) && other.processNext(id, bytes), "single-block pool round trip");
    require(id == 7 && bytes == std::vector<unsigned char>{0xff}, "single-byte binary preservation");
}

int main() {
    test_queue();
    test_packet_queue();
    std::cout << "PASS: Queue FIFO, wraparound, boundaries, packet order, binary data, and pool reuse.\n";
}
