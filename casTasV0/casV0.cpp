#include <atomic>
#include <iostream>
#include <thread>

//std::atomic<int> counter = 0;
std::atomic<int> counter{0};

void cas_increment(int id) {
    for (int i = 0; i < 5; ++i) {
        int expected;
        int desired;
        do {
            expected = counter.load();
            desired = expected + 1;
        } while (!counter.compare_exchange_weak(expected, desired));
        
        std::cout << "[CAS] Thread " << id << " incremented counter to " << desired << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    std::thread t1(cas_increment, 1);
    std::thread t2(cas_increment, 2);

    t1.join();
    t2.join();

    std::cout << "Final counter: " << counter << "\n";
    return 0;
}

