#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <chrono>

const int MAX_BUFFER_SIZE = 10;
std::queue<int> buffer;
std::mutex mtx;
std::condition_variable cv_producer, cv_consumer;

bool stop = false; // 控制标志
int item_id = 0;

void producer(int id) {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);

        // 如果 stop，则退出
        if (stop) break;

        // 等待直到缓冲区有空位或停止标志
        cv_producer.wait(lock, [] {
            return buffer.size() < MAX_BUFFER_SIZE || stop;
        });

        if (stop) break;

        int item = ++item_id;
        buffer.push(item);
        std::cout << "[Producer " << id << "] produced: " << item << "\n";

        lock.unlock();
        cv_consumer.notify_one();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[Producer " << id << "] exiting\n";
}

void consumer(int id) {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);

        // 如果停止且队列为空，退出
        cv_consumer.wait(lock, [] {
            return !buffer.empty() || stop;
        });

        if (buffer.empty() && stop) break;

        if (!buffer.empty()) {
            int item = buffer.front();
            buffer.pop();
            std::cout << "    [Consumer " << id << "] consumed: " << item << "\n";
            lock.unlock();
            cv_producer.notify_one();
        } else {
            lock.unlock(); // nothing to consume, release lock
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    std::cout << "    [Consumer " << id << "] exiting\n";
}

int main() {
    const int num_producers = 3;
    const int num_consumers = 3;

    std::vector<std::thread> producers, consumers;

    for (int i = 0; i < num_producers; ++i)
        producers.emplace_back(producer, i);

    for (int i = 0; i < num_consumers; ++i)
        consumers.emplace_back(consumer, i);

    // 运行一段时间后设置 stop = true
    std::this_thread::sleep_for(std::chrono::seconds(5));
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop = true;
    }

    // 唤醒所有线程
    cv_producer.notify_all();
    cv_consumer.notify_all();

    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();

    std::cout << "All threads exited.\n";
    return 0;
}
