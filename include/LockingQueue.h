#ifndef LOCKINGQUEUE_H
#define LOCKINGQUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

template<typename T>
class LockingQueue {
public:
    ~LockingQueue() { close(); }

    // Move item into the queue and wake one waiting pop() call.
    void push(T item) {
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_queue.push(std::move(item));
        }
        m_cv.notify_one();
    }

    // Block until an item is available or the queue is closed and drained.
    // On success, out receives the item and true is returned.
    // Returns false when the queue is permanently empty (closed + drained).
    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [this] { return !m_queue.empty() || m_closed; });
        if (m_queue.empty()) {
            return false;
        }
        out = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    // Signal that no more pushes will occur; unblocks all waiting pop() calls.
    void close() {
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_closed = true;
        }
        m_cv.notify_all();
    }

private:
    std::mutex              m_mtx;
    std::condition_variable m_cv;
    std::queue<T>           m_queue;
    bool                    m_closed = false;
};

#endif // LOCKINGQUEUE_H
