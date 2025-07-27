#include <iostream>
#include <semaphore.h>
#include <thread>
#include <unistd.h>

sem_t sem;

void access_resource(int id) {
    sem_wait(&sem);  // wait (decrement)

    std::cout << "Thread " << id << " entering critical section\n";
    sleep(1);  // simulate work
    std::cout << "Thread " << id << " leaving critical section\n";

    sem_post(&sem);  // signal (increment)
}

int main() {
    sem_init(&sem, 0, 3);  // 3 threads allowed in at once

    std::thread t[10];
    for (int i = 0; i < 10; ++i) {
        t[i] = std::thread(access_resource, i);
    }

    for (int i = 0; i < 10; ++i) {
        t[i].join();
    }

    sem_destroy(&sem);
    return 0;
}

