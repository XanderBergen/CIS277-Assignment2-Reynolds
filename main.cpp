#include "MemoryPool.h"
#include "PacketQueue.h"
#include "Queue.h"

#include <array>
#include <cstddef>
#include <deque>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// This is a demonstration AND an independent test driver.
// std::deque is used ONLY as a reference model in the tests; Queue<T>
// itself is implemented in Queue.h using a fixed-capacity circular array.

using Bytes = std::vector<unsigned char>;

std::string hexBytes(const Bytes& bytes)
{
    std::ostringstream output;
    output << '[' << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0)
        {
            output << ' ';
        }
        output << "0x" << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    output << ']';
    return output.str();
}

template <typename T>
std::string describe(const T& value)
{
    std::ostringstream output;
    output << std::boolalpha << value;
    return output.str();
}

std::string describe(const Bytes& value)
{
    return hexBytes(value);
}

struct Counts
{
    std::size_t pending;
    std::size_t available;

    bool operator==(const Counts&) const = default;
};

std::string describe(const Counts& counts)
{
    return "pending=" + std::to_string(counts.pending) +
        ", available=" + std::to_string(counts.available);
}

Counts countsOf(const PacketQueue& packets)
{
    return { packets.pending(), packets.availableBlocks() };
}

class TestRunner
{
private:
    std::size_t passed_ = 0;
    std::size_t failed_ = 0;

public:
    void section(const std::string& name) const
    {
        std::cout << "\n--- " << name << " ---\n";
    }

    template <typename Expected, typename Actual>
    void equal(const std::string& name, const Expected& expected,
        const Actual& actual)
    {
        const bool success = (expected == actual);
        std::cout << (success ? "[PASS] " : "[FAIL] ") << name
            << " | expected: " << describe(expected)
            << " | actual: " << describe(actual) << '\n';
        if (success)
        {
            ++passed_;
        }
        else
        {
            ++failed_;
        }
    }

    void check(const std::string& name, bool success,
        const std::string& expected,
        const std::string& actual)
    {
        std::cout << (success ? "[PASS] " : "[FAIL] ") << name
            << " | expected: " << expected
            << " | actual: " << actual << '\n';
        if (success)
        {
            ++passed_;
        }
        else
        {
            ++failed_;
        }
    }

    int summary() const
    {
        std::cout << "\n========================================\n"
            << "RESULTS: " << passed_ << " passed, " << failed_
            << " failed\n"
            << (failed_ == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n")
            << "========================================\n";
        return failed_ == 0 ? 0 : 1;
    }
};

void demonstration(TestRunner& tests)
{
    tests.section("REQUIRED PACKET DEMONSTRATION");
    PacketQueue packets(8, 3);

    const Bytes first{ 0x00, 0xFF, 0x10 };
    const Bytes second{ 0xAB, 0x00, 0x7E, 0xFF };
    const Bytes third{ 0x39 };
    const Bytes fourth{ 0x80, 0x01 };
    const Bytes extra{ 0x55 };

    auto receiveAndShow = [&](std::size_t id, const Bytes& data)
        {
            const bool accepted = packets.receive(id, data.data(), data.size());
            std::cout << "receive(" << id << ", " << hexBytes(data) << ") -> "
                << std::boolalpha << accepted << " | "
                << describe(countsOf(packets)) << '\n';
            return accepted;
        };

    tests.equal("Receive packet 101", true, receiveAndShow(101, first));
    tests.equal("Receive packet 102", true, receiveAndShow(102, second));
    tests.equal("Receive packet 103", true, receiveAndShow(103, third));
    tests.equal("Queue reaches capacity", Counts{ 3, 0 }, countsOf(packets));

    const Counts fullBefore = countsOf(packets);
    tests.equal("Reject receive when full", false, receiveAndShow(999, extra));
    tests.equal("Full receive leaves counts unchanged", fullBefore,
        countsOf(packets));

    auto processAndShow = [&](std::size_t expectedId, const Bytes& expectedBytes)
        {
            std::size_t id = 0;
            Bytes bytes{ 0xCC }; // Verify output is replaced rather than appended.
            const bool processed = packets.processNext(id, bytes);
            std::cout << "processNext() -> " << std::boolalpha << processed
                << ", id=" << id << ", bytes=" << hexBytes(bytes)
                << " | " << describe(countsOf(packets)) << '\n';
            tests.equal("Process returns true for packet " + std::to_string(expectedId),
                true, processed);
            tests.equal("FIFO ID " + std::to_string(expectedId), expectedId, id);
            tests.equal("Payload for packet " + std::to_string(expectedId),
                expectedBytes, bytes);
        };

    processAndShow(101, first);
    tests.equal("One block returned after processing", Counts{ 2, 1 },
        countsOf(packets));

    // MemoryPool may reuse the released block immediately (LIFO).
    // The circular Queue still delivers 102, 103, then 104 (FIFO).
    tests.equal("Receive packet 104 after releasing a block", true,
        receiveAndShow(104, fourth));
    tests.equal("Wrapped/full packet counts", Counts{ 3, 0 }, countsOf(packets));

    processAndShow(102, second);
    processAndShow(103, third);
    processAndShow(104, fourth);
    tests.equal("All blocks returned after draining", Counts{ 0, 3 },
        countsOf(packets));

    std::size_t unchangedId = 777;
    Bytes unchangedBytes{ 0xDE, 0xAD };
    tests.equal("Processing empty packet queue returns false", false,
        packets.processNext(unchangedId, unchangedBytes));
    tests.equal("Empty processing preserves output ID", std::size_t{ 777 },
        unchangedId);
    tests.equal("Empty processing preserves output bytes", Bytes{ 0xDE, 0xAD },
        unchangedBytes);
    tests.equal("Empty processing preserves counts", Counts{ 0, 3 },
        countsOf(packets));

    std::cout << "Note: the pool reuses free blocks in LIFO order, "
        "but packet service remains FIFO.\n";
}

void testIntegerQueue(TestRunner& tests)
{
    tests.section("QUEUE<int>: CAPACITY, OVERFLOW, UNDERFLOW, WRAPAROUND");
    Queue<int> queue(3);
    tests.equal("Initially empty", true, queue.empty());
    tests.equal("Initially not full", false, queue.full());
    tests.equal("Initially size zero", std::size_t{ 0 }, queue.size());
    tests.equal("Fixed capacity", std::size_t{ 3 }, queue.capacity());

    int value = -777;
    tests.equal("front on empty returns false", false, queue.front(value));
    tests.equal("front on empty preserves value", -777, value);
    tests.equal("dequeue on empty returns false", false, queue.dequeue(value));
    tests.equal("dequeue on empty preserves value", -777, value);

    tests.equal("enqueue 10", true, queue.enqueue(10));
    tests.equal("enqueue 20", true, queue.enqueue(20));
    tests.equal("enqueue 30", true, queue.enqueue(30));
    tests.equal("Full is true", true, queue.full());
    tests.equal("Size when full", std::size_t{ 3 }, queue.size());
    tests.equal("enqueue 40 while full returns false", false, queue.enqueue(40));
    tests.equal("Overflow preserves size", std::size_t{ 3 }, queue.size());

    const Queue<int>& constQueue = queue;
    tests.equal("const front succeeds", true, constQueue.front(value));
    tests.equal("front observes oldest element", 10, value);
    tests.equal("front does not remove element", std::size_t{ 3 }, queue.size());
    tests.equal("Repeated front succeeds", true, constQueue.front(value));
    tests.equal("Repeated front returns same element", 10, value);

    tests.equal("Dequeue first succeeds", true, queue.dequeue(value));
    tests.equal("Oldest item dequeued first", 10, value);
    tests.equal("Not full after dequeue", false, queue.full());
    tests.equal("enqueue 40 after dequeue wraps", true, queue.enqueue(40));
    for (int expected : {20, 30, 40})
    {
        const bool removed = queue.dequeue(value);
        tests.equal("Wrapped dequeue succeeds (" + std::to_string(expected) + ")",
            true, removed);
        tests.equal("Wrapped FIFO value", expected, value);
    }
    tests.equal("Empty after complete drain", true, queue.empty());
    tests.equal("Size after drain", std::size_t{ 0 }, queue.size());
    value = 12345;
    tests.equal("Repeated empty dequeue returns false", false,
        queue.dequeue(value));
    tests.equal("Repeated underflow preserves output", 12345, value);
    tests.equal("Refill after drain succeeds", true, queue.enqueue(50));
    tests.equal("Refill dequeue succeeds", true, queue.dequeue(value));
    tests.equal("Refill returns correct value", 50, value);
}

void testStringAndSingleSlotQueue(TestRunner& tests)
{
    tests.section("QUEUE<std::string> AND SINGLE-SLOT BOUNDARIES");
    Queue<std::string> words(2);
    std::string output = "unchanged";
    tests.equal("Empty string front rejects", false, words.front(output));
    tests.equal("Empty string front keeps output", std::string("unchanged"),
        output);
    tests.equal("Enqueue alpha", true, words.enqueue("alpha"));
    tests.equal("Enqueue beta", true, words.enqueue("beta"));
    tests.equal("Full string queue rejects gamma", false,
        words.enqueue("gamma"));
    tests.equal("String dequeue alpha", true, words.dequeue(output));
    tests.equal("String FIFO alpha", std::string("alpha"), output);
    tests.equal("String circular enqueue gamma", true,
        words.enqueue("gamma"));
    tests.equal("String dequeue beta", true, words.dequeue(output));
    tests.equal("String FIFO beta", std::string("beta"), output);
    tests.equal("String dequeue gamma", true, words.dequeue(output));
    tests.equal("String FIFO gamma", std::string("gamma"), output);
    tests.equal("Empty string dequeue rejects", false, words.dequeue(output));
    tests.equal("String underflow preserves output", std::string("gamma"),
        output);

    Queue<int> single(1);
    int item = -1;
    tests.equal("Capacity-one starts empty", true, single.empty());
    tests.equal("Capacity-one enqueue", true, single.enqueue(7));
    tests.equal("Capacity-one becomes full", true, single.full());
    tests.equal("Capacity-one overflow", false, single.enqueue(8));
    tests.equal("Capacity-one dequeue", true, single.dequeue(item));
    tests.equal("Capacity-one value", 7, item);
    tests.equal("Capacity-one wraps and refills", true, single.enqueue(9));
    tests.equal("Capacity-one second dequeue", true, single.dequeue(item));
    tests.equal("Capacity-one reused-slot value", 9, item);
}

void testQueueStress(TestRunner& tests)
{
    tests.section("DETERMINISTIC QUEUE STRESS / REFERENCE COMPARISON");
    std::mt19937 generator(2772026);

    for (std::size_t limit : std::array<std::size_t, 5>{ 1, 2, 3, 7, 32 })
    {
        Queue<int> queue(limit);
        std::deque<int> reference;
        bool allCorrect = true;
        std::size_t completed = 0;

        for (std::size_t step = 0; step < 10000; ++step)
        {
            const unsigned int operation = generator() % 100U;
            int actual = -999;

            if (operation < 46U)
            {
                const int next = static_cast<int>(step * 13U + limit);
                const bool expected = reference.size() < limit;
                const bool accepted = queue.enqueue(next);
                allCorrect = allCorrect && (accepted == expected);
                if (expected)
                {
                    reference.push_back(next);
                }
            }
            else if (operation < 85U)
            {
                const bool expected = !reference.empty();
                const bool removed = queue.dequeue(actual);
                allCorrect = allCorrect && (removed == expected);
                if (expected)
                {
                    allCorrect = allCorrect && actual == reference.front();
                    reference.pop_front();
                }
                else
                {
                    allCorrect = allCorrect && actual == -999;
                }
            }
            else
            {
                const bool expected = !reference.empty();
                const bool peeked = queue.front(actual);
                allCorrect = allCorrect && (peeked == expected);
                allCorrect = allCorrect &&
                    (actual == (expected ? reference.front() : -999));
            }

            allCorrect = allCorrect && queue.size() == reference.size() &&
                queue.capacity() == limit &&
                queue.empty() == reference.empty() &&
                queue.full() == (reference.size() == limit);
            ++completed;
            if (!allCorrect)
            {
                break;
            }
        }

        tests.check("Queue capacity " + std::to_string(limit) +
            ": randomized FIFO/invariants",
            allCorrect, "10000 operations match reference",
            std::to_string(completed) + " matching operations");
    }
}

void testMemoryPool(TestRunner& tests)
{
    tests.section("MEMORY POOL: ALLOCATION, REUSE, INVALID FREE");
    MemoryPool pool(4, 3);
    tests.equal("Initially three free blocks", std::size_t{ 3 },
        pool.availableBlocks());
    tests.equal("Initially zero allocated blocks", std::size_t{ 0 },
        pool.allocatedBlocks());
    tests.equal("Pool block size", std::size_t{ 4 }, pool.blockSize());
    tests.equal("Pool byte capacity", std::size_t{ 12 }, pool.capacity());

    void* first = pool.allocate();
    void* second = pool.allocate();
    void* third = pool.allocate();
    tests.equal("Three unique blocks allocated",
        true, first != nullptr && second != nullptr &&
        third != nullptr && first != second &&
        first != third && second != third);
    tests.equal("Pool exhausted returns nullptr", true,
        pool.allocate() == nullptr);
    tests.equal("No free blocks while exhausted", std::size_t{ 0 },
        pool.availableBlocks());

    unsigned char unrelated[4]{};
    tests.equal("Null pointer deallocation rejected", false,
        pool.deallocate(nullptr));
    tests.equal("Foreign pointer deallocation rejected", false,
        pool.deallocate(unrelated));
    tests.equal("Interior pointer deallocation rejected", false,
        pool.deallocate(static_cast<unsigned char*>(first) + 1));
    tests.equal("Invalid frees do not change free count", std::size_t{ 0 },
        pool.availableBlocks());

    tests.equal("Free middle block", true, pool.deallocate(second));
    tests.equal("Double-free rejected", false, pool.deallocate(second));
    tests.equal("One block free", std::size_t{ 1 }, pool.availableBlocks());
    void* reused = pool.allocate();
    tests.equal("Next allocation reuses last freed block (LIFO)", true,
        reused == second);
    tests.equal("All blocks allocated again", std::size_t{ 3 },
        pool.allocatedBlocks());
    tests.equal("Release first block", true, pool.deallocate(first));
    tests.equal("Release third block", true, pool.deallocate(third));
    tests.equal("Release reused block", true, pool.deallocate(reused));
    tests.equal("All blocks free", std::size_t{ 3 }, pool.availableBlocks());
    tests.equal("Zero blocks still allocated", std::size_t{ 0 },
        pool.allocatedBlocks());
}

void testPacketBoundaries(TestRunner& tests)
{
    tests.section("PACKET QUEUE: INVALID INPUTS AND PAYLOAD BOUNDARIES");
    PacketQueue packets(4, 2);
    const Bytes shortPacket{ 0x00 };
    const Bytes fullPacket{ 0x00, 0xFF, 0x01, 0x7F };
    const Bytes tooLarge{ 0x00, 0x01, 0x02, 0x03, 0x04 };
    const Counts initial{ 0, 2 };

    tests.equal("Initially no packets and two blocks", initial,
        countsOf(packets));
    tests.equal("Null pointer rejected", false, packets.receive(1, nullptr, 1));
    tests.equal("Null pointer rejection keeps state", initial,
        countsOf(packets));
    tests.equal("Zero-length packet rejected", false,
        packets.receive(2, shortPacket.data(), 0));
    tests.equal("Zero-length rejection keeps state", initial,
        countsOf(packets));
    tests.equal("Oversize packet rejected", false,
        packets.receive(3, tooLarge.data(), tooLarge.size()));
    tests.equal("Oversize rejection keeps state", initial,
        countsOf(packets));

    tests.equal("Accept one-byte packet (minimum)", true,
        packets.receive(0, shortPacket.data(), shortPacket.size()));
    tests.equal("Accept exactly block-size packet", true,
        packets.receive(4, fullPacket.data(), fullPacket.size()));
    tests.equal("Both queue slots occupied", Counts{ 2, 0 }, countsOf(packets));
    tests.equal("Reject valid packet at capacity", false,
        packets.receive(5, shortPacket.data(), shortPacket.size()));
    tests.equal("Overflow preserves packet counts", Counts{ 2, 0 },
        countsOf(packets));

    std::size_t id = 999;
    Bytes bytes{ 0xCC };
    tests.equal("Process one-byte packet", true, packets.processNext(id, bytes));
    tests.equal("Packet ID may be zero", std::size_t{ 0 }, id);
    tests.equal("One-byte packet preserved", shortPacket, bytes);
    tests.equal("One block freed after processing", Counts{ 1, 1 },
        countsOf(packets));

    tests.equal("Process exact-size packet", true,
        packets.processNext(id, bytes));
    tests.equal("Exact-size packet ID", std::size_t{ 4 }, id);
    tests.equal("Exact-size binary packet preserved", fullPacket, bytes);
    tests.equal("Fully drained packet counts", initial, countsOf(packets));

    id = 999;
    bytes = { 0xAA, 0xBB };
    tests.equal("Empty processNext returns false", false,
        packets.processNext(id, bytes));
    tests.equal("Empty processNext preserves ID", std::size_t{ 999 }, id);
    tests.equal("Empty processNext preserves vector", Bytes({ 0xAA, 0xBB }),
        bytes);
    tests.equal("Empty processNext preserves counts", initial,
        countsOf(packets));
}

void testPacketOwnershipAndIsolation(TestRunner& tests)
{
    tests.section("PACKET QUEUE: INDEPENDENT COPIES AND MULTIPLE INSTANCES");
    PacketQueue left(6, 1);
    PacketQueue right(6, 2);
    Bytes source{ 0x00, 0xFF, 0x23 };
    const Bytes original = source;

    tests.equal("Receive copies the source bytes", true,
        left.receive(77, source.data(), source.size()));
    source.assign({ 0x44, 0x55, 0x66 }); // Must not alter stored packet.
    tests.equal("Second PacketQueue accepts own packet", true,
        right.receive(88, source.data(), source.size()));
    tests.equal("Separate queues maintain independent counts", Counts{ 1, 0 },
        countsOf(left));
    tests.equal("Second queue counts", Counts{ 1, 1 }, countsOf(right));

    std::size_t id = 0;
    Bytes received;
    tests.equal("Process first independent queue", true,
        left.processNext(id, received));
    tests.equal("Original data unaffected by source mutation", original,
        received);
    tests.equal("Original packet ID", std::size_t{ 77 }, id);
    tests.equal("First queue now empty", Counts{ 0, 1 }, countsOf(left));
    tests.equal("Second queue still pending", Counts{ 1, 1 }, countsOf(right));

    tests.equal("Process second independent queue", true,
        right.processNext(id, received));
    tests.equal("Second queue has its own source bytes", source, received);
    tests.equal("Second packet ID", std::size_t{ 88 }, id);

    // The assignment does not require unique packet IDs. Arrival order
    // must be maintained even if two IDs happen to be equal.
    PacketQueue duplicates(2, 2);
    const Bytes a{ 0x01 };
    const Bytes b{ 0x02 };
    tests.equal("Accept first repeated ID", true,
        duplicates.receive(5, a.data(), a.size()));
    tests.equal("Accept second repeated ID", true,
        duplicates.receive(5, b.data(), b.size()));
    duplicates.processNext(id, received);
    tests.equal("First repeated-ID payload", a, received);
    duplicates.processNext(id, received);
    tests.equal("Second repeated-ID payload", b, received);
}

struct ExpectedPacket
{
    std::size_t id;
    Bytes bytes;
};

void testPacketStress(TestRunner& tests)
{
    tests.section("DETERMINISTIC PACKET FIFO / INVALID-INPUT STRESS");
    std::mt19937 generator(277123);
    constexpr std::size_t blockSize = 8;

    for (std::size_t limit : std::array<std::size_t, 3>{ 1, 3, 8 })
    {
        PacketQueue packets(blockSize, limit);
        std::deque<ExpectedPacket> reference;
        bool allCorrect = true;
        std::size_t completed = 0;

        for (std::size_t step = 0; step < 10000; ++step)
        {
            const unsigned int action = generator() % 100U;
            const std::size_t length = 1 + generator() % blockSize;
            Bytes payload(length);
            for (std::size_t i = 0; i < length; ++i)
            {
                payload[i] = static_cast<unsigned char>(generator() % 256U);
            }
            payload[0] = 0x00;
            if (payload.size() > 1)
            {
                payload.back() = 0xFF;
            }
            const std::size_t id = step % 19 == 0 ? 0 : step + 1;
            const Counts before = countsOf(packets);

            if (action < 52U) // Valid receive, sometimes against full pool.
            {
                const bool expected = reference.size() < limit;
                const bool accepted = packets.receive(id, payload.data(),
                    payload.size());
                allCorrect = allCorrect && accepted == expected;
                if (accepted)
                {
                    reference.push_back({ id, payload });
                }
            }
            else if (action < 62U) // Invalid null pointer.
            {
                allCorrect = allCorrect && !packets.receive(id, nullptr, 1);
                allCorrect = allCorrect && countsOf(packets) == before;
            }
            else if (action < 72U) // Invalid zero length.
            {
                allCorrect = allCorrect &&
                    !packets.receive(id, payload.data(), 0);
                allCorrect = allCorrect && countsOf(packets) == before;
            }
            else if (action < 82U) // Invalid oversize length.
            {
                const Bytes oversized(blockSize + 1, 0xFF);
                allCorrect = allCorrect &&
                    !packets.receive(id, oversized.data(),
                        oversized.size());
                allCorrect = allCorrect && countsOf(packets) == before;
            }
            else // Process next or try to process an empty queue.
            {
                std::size_t outputId = 999999;
                Bytes outputBytes{ 0xDE, 0xAD };
                const bool expected = !reference.empty();
                const bool removed = packets.processNext(outputId, outputBytes);
                allCorrect = allCorrect && removed == expected;
                if (expected)
                {
                    allCorrect = allCorrect &&
                        outputId == reference.front().id &&
                        outputBytes == reference.front().bytes;
                    reference.pop_front();
                }
                else
                {
                    allCorrect = allCorrect && outputId == 999999 &&
                        outputBytes == Bytes({ 0xDE, 0xAD }) &&
                        countsOf(packets) == before;
                }
            }

            const Counts current = countsOf(packets);
            allCorrect = allCorrect && current.pending == reference.size() &&
                current.available == limit - reference.size();
            ++completed;
            if (!allCorrect)
            {
                break;
            }
        }

        // Even after all random operations, remaining packets must drain FIFO.
        while (allCorrect && !reference.empty())
        {
            std::size_t id = 0;
            Bytes bytes;
            const bool removed = packets.processNext(id, bytes);
            allCorrect = allCorrect && removed &&
                id == reference.front().id &&
                bytes == reference.front().bytes;
            reference.pop_front();
        }
        allCorrect = allCorrect && countsOf(packets) == Counts{ 0, limit };

        tests.check("Packet capacity " + std::to_string(limit) +
            ": randomized FIFO/data/counts/rejections",
            allCorrect, "10000 operations + FIFO drain match reference",
            std::to_string(completed) +
            " matching operations + " +
            (allCorrect ? "correct drain" : "mismatch"));
    }
}

int main()
{
    TestRunner tests;
    demonstration(tests);
    testIntegerQueue(tests);
    testStringAndSingleSlotQueue(tests);
    testQueueStress(tests);
    testMemoryPool(tests);
    testPacketBoundaries(tests);
    testPacketOwnershipAndIsolation(tests);
    testPacketStress(tests);
    return tests.summary();
}
