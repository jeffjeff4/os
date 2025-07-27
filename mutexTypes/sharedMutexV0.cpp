#include <iostream>
#include <shared_mutex>
#include <thread>
#include <chrono>

std::shared_mutex shmtx;

void reader(int id) {
    std::shared_lock lock(shmtx);
    std::cout << "Reader " << id << " reading..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

void writer(int id) {
    std::unique_lock lock(shmtx);
    std::cout << "Writer " << id << " writing..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main() {
    std::thread r1(reader, 1), r2(reader, 2), w1(writer, 1);

    r1.join();
    r2.join();
    w1.join();
    return 0;
}

