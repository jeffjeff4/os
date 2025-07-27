#include <iostream>
#include <mutex>

std::recursive_mutex rmtx;

void recursive_function(int count) {
    if (count <= 0) return;

    rmtx.lock();
    std::cout << "Lock level: " << count << std::endl;
    recursive_function(count - 1);
    rmtx.unlock();
}

int main() {
    recursive_function(3);
    return 0;
}

