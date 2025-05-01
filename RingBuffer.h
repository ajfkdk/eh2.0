#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <array>
#include <mutex>
#include <atomic>
#include <condition_variable>

template <typename T, size_t Size>
class RingBuffer {
private:
    std::array<T, Size> buffer;
    std::atomic<size_t> writeIndex{ 0 };
    std::atomic<size_t> readIndex{ 0 };
    std::atomic<size_t> count{ 0 };
    std::mutex mutex;
    std::condition_variable notEmpty;
    std::condition_variable notFull;
    std::atomic<bool> closed{ false };

public:
    // 写入新数据
    bool write(const T& item, bool waitIfFull = false) {
        if (closed.load()) return false;

        if (waitIfFull) {
            std::unique_lock<std::mutex> lock(mutex);
            notFull.wait(lock, [this] {
                return count.load() < Size || closed.load();
                });
            if (closed.load()) return false;
        }
        else {
            if (count.load() >= Size) {
                // 缓冲区满，移动读索引丢弃最老的数据
                readIndex.store((readIndex.load() + 1) % Size);
                count.fetch_sub(1);
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            buffer[writeIndex] = item;
            writeIndex = (writeIndex + 1) % Size;
            count.fetch_add(1);
        }
        notEmpty.notify_one();
        return true;
    }

    // 读取数据
    bool read(T& item, bool waitIfEmpty = false) {
        if (closed.load() && count.load() == 0) return false;

        if (waitIfEmpty) {
            std::unique_lock<std::mutex> lock(mutex);
            notEmpty.wait(lock, [this] {
                return count.load() > 0 || closed.load();
                });
            if (count.load() == 0 && closed.load()) return false;
        }
        else {
            if (count.load() == 0) return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            item = buffer[readIndex];
            readIndex = (readIndex + 1) % Size;
            count.fetch_sub(1);
        }
        notFull.notify_one();
        return true;
    }

    // 关闭缓冲区
    void close() {
        closed.store(true);
        notEmpty.notify_all();
        notFull.notify_all();
    }

    // 重新打开缓冲区
    void open() {
        closed.store(false);
    }

    // 清空缓冲区
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        readIndex.store(0);
        writeIndex.store(0);
        count.store(0);
    }

    // 获取当前条目数
    size_t size() const {
        return count.load();
    }

    // 判断是否已关闭
    bool isClosed() const {
        return closed.load();
    }
};

#endif // RING_BUFFER_H
