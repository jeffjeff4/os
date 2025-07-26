#include <atomic>
#include <iostream>
#include <thread>

std::atomic_flag tas_lock = ATOMIC_FLAG_INIT;

void tas_critical_section(int id) {
    while (tas_lock.test_and_set(std::memory_order_acquire)) {
        // Busy wait (spin) until lock is free
    }

    // Critical section
    std::cout << "[TAS] Thread " << id << " entered critical section.\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "[TAS] Thread " << id << " leaving critical section.\n";

    tas_lock.clear(std::memory_order_release);
}

int main() {
    std::thread t1(tas_critical_section, 1);
    std::thread t2(tas_critical_section, 2);

    t1.join();
    t2.join();
    return 0;
}

