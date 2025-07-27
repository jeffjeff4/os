#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>

std::timed_mutex tmtx;

void task(int idx) {
    if (tmtx.try_lock_for(std::chrono::milliseconds(100))) {
        std::cout << "idx = "<< idx<<", Got lock!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tmtx.unlock();
        std::cout << "idx = "<< idx <<", get unlocked" << std::endl;
    } else {
        std::cout << "idx = "<< idx <<", Failed to get lock (timeout)" << std::endl;
    }
}

int main() {
    std::thread t1(task, 0);
    std::thread t2(task, 1);

    t1.join();
    t2.join();
    return 0;
}

