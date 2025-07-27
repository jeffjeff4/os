#include <iostream>
#include <mutex>
#include <thread>

std::mutex mtx;

void print_safe(const std::string& msg) {
    mtx.lock();
    std::cout << msg << std::endl;
    mtx.unlock();
}

int main() {
    std::thread t1(print_safe, "Hello from t1");
    std::thread t2(print_safe, "Hello from t2");

    t1.join();
    t2.join();
    return 0;
}

