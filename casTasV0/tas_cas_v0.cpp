#include <atomic>
#include <iostream>
#include <thread>


//🧠 Summary
//Operation                   Primitive Used                           Description
//TAS                         atomic_flag::test_and_set()              Sets the flag and returns its old value — used for spinlocks.
//CAS                         atomic<T>::compare_exchange_*()          Updates value only if it hasn't changed — lock-free programming.

// TAS Lock
std::atomic_flag tas_lock = ATOMIC_FLAG_INIT;

// CAS Counter
std::atomic<int> counter {0};

void tas_critical_section(int id) {
    while (tas_lock.test_and_set(std::memory_order_acquire)) {}
    std::cout << "[TAS] Thread " << id << " in critical section\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "[TAS] Thread " << id << " leaving\n";
    tas_lock.clear(std::memory_order_release);
}

void cas_increment(int id) {
    for (int i = 0; i < 3; ++i) {
        int expected, desired;
        do {
            expected = counter.load();
            desired = expected + 1;
        } while (!counter.compare_exchange_weak(expected, desired));
        std::cout << "[CAS] Thread " << id << " incremented to " << desired << "\n";
    }
}

int main() {
    std::thread t1(tas_critical_section, 1);
    std::thread t2(tas_critical_section, 2);
    t1.join(); t2.join();

    std::thread c1(cas_increment, 1);
    std::thread c2(cas_increment, 2);
    c1.join(); c2.join();

    std::cout << "Final counter: " << counter << "\n";
    return 0;
}

