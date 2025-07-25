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


/*
Part 1
How cv.wait Works Internally
cv.wait(lock, [] { return ready; });
Is equivalent to:

while (!ready) {
    cv.wait(lock);
}

it could help:
1. spurious wakeup
2. Missed notifications
*/

/*
1. how to avoid spurious wakeup
This while loop means:
If spurious wakeup happens and ready == false, go back to sleep.
If notify_one() happens and ready == false, go back to sleep.
Only when ready == true will the thread continue.

//BAD EXAMPLE — No predicate, vulnerable to spurious wakeup:
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void consumer() {
    std::unique_lock<std::mutex> lock(mtx);
    std::cout << "[Consumer] Waiting...\n";
    cv.wait(lock);  // ❌ unsafe — no predicate!
    // ❗ If spurious wakeup happens, code continues:
    if (!ready) {
        std::cout << "[Consumer] Spurious wakeup! Still not ready!\n";
    } else {
        std::cout << "[Consumer] Got ready!\n";
    }
}

void producer() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
        std::cout << "[Producer] Setting ready = true\n";
    }
    cv.notify_one();
}

int main() {
    std::thread t1(consumer);
    std::thread t2(producer);

    t1.join();
    t2.join();
}
*/

/*
2. how to avoid Missed notifications

How do we make sure producer calls notify_one() before a consumer starts waiting, to avoid missed notifications?

The short answer is:
✅ In this specific code: you don’t need to guarantee that, because it is already safe, due to the use of a predicate inside cv.wait(lock, predicate).

🧠 Why It’s Safe: Because of the Predicate in cv.wait(...)
Here’s the line from the consumer() function:

cv_consumer.wait(lock, [] {
    return !buffer.empty() || stop;
});
This form of wait is called predicate-based wait, and it rechecks the condition after each wakeup. 

It protects against:
1. Spurious wakeups
2. Missed notifications

🔒 Key behavior of cv.wait(lock, predicate):
Atomically unlocks the mutex and suspends the thread.

When notified, re-locks the mutex and re-checks the predicate.

If the predicate is false, it waits again.

So even if notify_one() happens before the thread calls wait(), it’s safe — because wait() won’t block if the condition is already true.

✅ Example to Prove It's Safe
Let’s say this happens:

Time			Producer Thread				Consumer Thread
T1			Push item into buffer	
T2			notify_one()	
T3												Lock mutex
T4											Call wait(lock, pred)
T5											Predicate is already true → return immediately

✔ So no notification is missed, even though the consumer started waiting after the notify.

❌ What Would Be Unsafe?
If you wrote it like this:

cv_consumer.wait(lock);  // ❌ No predicate!
Then the thread would wait no matter what, and if notify_one() already happened, it would block forever.

🔁 Summary
Concept											Explanation
Predicate in wait()							Prevents blocking if condition already true
Notification sent before wait				✅ Safe, as long as predicate is used
Notification without predicate				❌ Dangerous — can block forever
Should we force wait-before-notify?			❌ Not necessary with predicates
Is this code correct?						✅ Yes! It’s robust and won’t miss notifications

*/


/*
part 2:
is notify_all and notify_one also using mutex?

✅ Short Answer:
notify_one() and notify_all() do not require the mutex to be locked.

But they interact with the mutex used by wait() — so you must coordinate them carefully to avoid missed notifications or race conditions.

🧠 Details:
std::condition_variable::notify_one() and notify_all():
Internally signal/wake up one or all waiting threads.

They do not lock or require a mutex themselves.

But: the threads being notified are all waiting using the same shared mutex.

So the cv.notify_*() call does not lock the mutex, but the waiting thread will lock it again after being woken up.

🧪 Example of Proper Use:

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void producer() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
    } // 🔓 lock released here
    cv.notify_one();  // ✅ No lock held, but safe
}

void consumer() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });  // ✅ will reacquire lock on wake-up
    // use shared data safely under lock
}

❗ What If You Notify Without Locking?
It’s allowed, but if you notify before the other thread has started waiting, the notification may be missed.

// Bad timing example:
ready = true;
cv.notify_one();  // 🔔 Notify happens before consumer starts waiting!
Now if the consumer calls cv.wait() after this line, it’ll block forever.

✅ Best Practice
Always follow this pattern:
1. Lock the mutex
2. Modify shared condition
3. Unlock
4. Notify

{
    std::lock_guard<std::mutex> lock(mtx);
    condition = true;
}
cv.notify_one();  // 🚨 Must happen *after* the condition is true

✅ Summary
Question	                                                        Answer
Do notify_one() / notify_all() use the mutex?	               ❌ No, they don't lock it themselves
Should you hold the lock when calling them?	                    ✅ Best practice: yes, to avoid race conditions
What happens on wake-up?	                                   The waiting thread relocks the mutex before continuing


*/


/*
part 3:

why using mutex could solve the race condition problem?

🔑 Why does using a mutex solve race condition problems in C++?

🧠 What Is a Race Condition?
A race condition occurs when:
Two or more threads access shared data at the same time, and
At least one of the accesses is a write, and
The outcome depends on the timing of how the threads are scheduled.

Race conditions are dangerous because they cause:
Nondeterministic bugs (hard to reproduce)
Corrupted data (e.g., partial writes)
Crashes or security issues

🔐 How a std::mutex Solves This
A std::mutex (short for mutual exclusion) ensures:

🔒 Only one thread at a time can access the protected section of code.

When a thread locks a mutex:
All other threads must wait before they can acquire the lock
The locked region becomes a critical section: no interference
When the thread releases the mutex (via unlock() or RAII destructors), another thread can acquire it.

🧪 Example Without Mutex → Race Condition

int counter = 0;

void increment() {
    for (int i = 0; i < 100000; ++i) {
        counter++;  // ⚠️ Race condition: read-modify-write is not atomic
    }
}

Two threads calling increment() might both read counter = 5, then both write 6. Final result: 100,000 increments may result in less than 200,000 total!

🔥 How the Race Happens
Thread A:                                         Thread B:
----------                                       ----------
Reads counter=5
                                                Reads counter=5
Increments to 6
                                                Increments to 6
Writes counter=6                                ❌ overwrites A's write!
✅ Solution: Use a Mutex

std::mutex mtx;
int counter = 0;

void increment() {
    for (int i = 0; i < 100000; ++i) {
        std::lock_guard<std::mutex> lock(mtx);  // 🔒 mutual exclusion
        counter++;  // ✅ safe: only one thread inside this block
    }
}

Now:
Only one thread can modify counter at a time.
Race condition is eliminated.
The result is consistent and deterministic.


📦 Real-World Analogy
Think of a mutex like a bathroom key in a shared office:
Only one person can enter the bathroom at a time.
Everyone must get the key first.
You wait outside if someone else is using it.


Without the key? 🚽 Two people could enter at the same time → disaster 😅

📌 Summary
Without mutex                                                               With Mutex
Multiple threads access shared data at the same time                Only one thread accesses data at a time
Causes race conditions                                              Prevents race conditions
Non-deterministic behavior                                          Deterministic, consistent results
Data corruption possible                                            Data integrity preserved
*/

/*
part 4:
why producer, consumer need to use the same lock? is it correct if producer, consumer use different lock?

Q: Why must producer and consumer use the same lock (mutex)?
Is it correct if they use different locks?

❌ Short Answer: No, they must use the same lock
If producer and consumer use different mutexes, the condition variable won’t work correctly, because:

std::condition_variable coordinates waiting and notification using a specific mutex — and both wait() and notify_*() must synchronize on that same mutex.

🧠 Key Concept
A std::condition_variable is not magic — it does not track conditions directly. Instead:
It assumes the condition is protected by a shared mutex
It uses that mutex to atomically release and reacquire the lock
It synchronizes access to the condition state


🔍 What Happens If You Use Different Locks?
Let’s say:
Producer uses mutex1
Consumer uses mutex2
Both share the same condition_variable

🔥 This breaks everything:

std::mutex producer_mutex;
std::mutex consumer_mutex;
std::condition_variable cv;
bool ready = false;

void producer() {
    std::lock_guard<std::mutex> lock(producer_mutex);
    ready = true;
    cv.notify_one();  // 🔔 using cv, but not synchronized with consumer
}

void consumer() {
    std::unique_lock<std::mutex> lock(consumer_mutex);
    cv.wait(lock, [] { return ready; });  // ❌ Undefined behavior!
}

🚨 Why This Is Wrong
cv.wait() expects to unlock the same mutex that protects ready — here it doesn't.

The consumer locks consumer_mutex, but the producer does not — so ready might be changed while consumer reads it!

This can lead to:
Missed wakeups
Race conditions on ready
Undefined behavior

✅ Correct Design
Producer and consumer must both lock the same std::mutex that protects the shared condition:

std::mutex mtx;                // shared mutex
std::condition_variable cv;    // shared cv
bool ready = false;

void producer() {
    {
        std::lock_guard<std::mutex> lock(mtx);  // ✅ same lock
        ready = true;
    }
    cv.notify_one();  // ✅ safe — protected by same lock
}

void consumer() {
    std::unique_lock<std::mutex> lock(mtx);  // ✅ same lock
    cv.wait(lock, [] { return ready; });     // ✅ safe wait
}

🧪 Visual Explanation
Shared state: `ready`
Protected by: `mtx`

Both threads must:
- Lock `mtx` when reading/writing `ready`
- Use `cv` to wait/notify

→ Without same `mtx`, no consistent access, no proper sync
✅ Summary
Scenario                                                        Result
✅ Both threads use same mutex                           Condition variable works safely
❌ Threads use different mutexes                         Race conditions and undefined behavior
❌ Only cv.notify_one() without locking                  Risk of missed notifications
✅ wait(lock, predicate) with same mutex                 Safe and correct
*/


/*
part 4:


What if no lock?    Race conditions, missed notifications, undefined behavior.
please give detailed examples


ChatGPT said:
Absolutely — let’s break it down with detailed C++ examples to show what can go wrong when you don’t use a lock with a condition_variable.

🧨 Problem: No Lock ⇒ Race Condition + Missed Notification
We'll walk through a scenario with:

Producer thread: pushes to a shared queue

Consumer thread: waits until queue is not empty

❌ Incorrect Code (No Lock)

#include <iostream>
#include <queue>
#include <thread>
#include <condition_variable>

std::queue<int> buffer;
std::condition_variable cv;
bool ready = false;

void producer() {
    buffer.push(42);
    ready = true;
    cv.notify_one(); // 👈 this might be missed!
}

void consumer() {
    if (!ready) {
        // ❌ NO LOCK — this is undefined behavior!
        cv.wait(std::unique_lock<std::mutex>(std::mutex{}));
    }
    std::cout << "[Consumer] Got: " << buffer.front() << "\n";
}

❗ Why This Is Broken
cv.wait(...) needs a valid, locked mutex

Here we create a new mutex and lock it on the spot — this mutex does not protect the shared state

There's no mutual exclusion on buffer or ready

Producer might call notify_one() before consumer waits — consumer will wait forever

🔥 Timeline of Failure

Time | Producer            | Consumer
-----|---------------------|--------------------------
 T0  |                     | checks `ready == false`
 T1  |                     | calls `cv.wait(...)`
 T2  | buffer.push(42)     | (waits forever)
 T3  | ready = true        |
 T4  | cv.notify_one()     | ❌ missed — no one was waiting yet
Result: Deadlock — consumer never wakes up, even though the data is ready.

✅ Correct Code with Proper Locking

#include <iostream>
#include <queue>
#include <thread>
#include <condition_variable>
#include <mutex>

std::queue<int> buffer;
std::condition_variable cv;
std::mutex mtx;
bool ready = false;

void producer() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        buffer.push(42);
        ready = true;
    }
    cv.notify_one();  // ✅ will not be missed
}

void consumer() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });  // ✅ safe check
    std::cout << "[Consumer] Got: " << buffer.front() << "\n";
}

✅ Why This Works
cv.wait(lock, predicate) ensures:

If ready is already true, the thread continues immediately.
If ready == false, the mutex is released and the thread sleeps.
When notified, it relocks the mutex and rechecks the predicate.

The mutex protects both buffer and ready.

🔑 Summary
Mistake What Happens
Not using mutex Data race: buffer accessed unsafely
Using unrelated/new mutex in wait() Thread waits on a mutex no one ever locks — broken logic
Calling notify_one() before wait()  Notification can be missed if wait() hasn’t started yet

🧠 Key Rule
cv.wait() must use a std::unique_lock<std::mutex> that protects the same shared data accessed by both threads.

*/

/*
part 5:

why cv.wait(lock, ...), why cv need lock?


🔍 Why does cv.wait(lock, ...) require a lock?

✅ Short Answer:
cv.wait(lock) needs a lock because:

It must release the lock while waiting and re-acquire it when notified — safely and atomically.

This is critical to avoid:
1. Data races
2. Missed notifications
3. Inconsistent shared state

🔄 What Happens Internally?
Here's what cv.wait(lock) does under the hood:

while (!condition()) {
    // Atomically:
    // 1. Unlock mutex
    // 2. Block the thread
    // 3. When notified, re-lock mutex
    cv.wait(lock);
}

🔑 Why is the Lock Needed?
Reason                                                          Explanation
🔐 Mutex protects shared data                         All threads access shared data (e.g., buffer, flags). The mutex ensures consistency.
💥 Avoid race conditions                              If a thread checks a condition without locking, it may get outdated or invalid data.
📦 Atomic wait-unlock                                 wait() releases the lock atomically while putting the thread to sleep.
🔁 Re-lock on wake                                    When notify_one() or notify_all() wakes the thread, it automatically re-locks the mutex before continuing.
❌ Prevent missed notifications                       If no lock: producer might notify before consumer even starts waiting → notification lost.

🧪 Example: Without Lock → ❌ Race Condition

// Unsafe: NOT using lock!
if (buffer.empty()) {
    cv.wait(???);  // no mutex? undefined behavior
}
What if a producer notifies just before this thread starts waiting? 😱

Notification is missed, thread blocks forever.

✅ Correct: Always Hold the Lock

std::unique_lock<std::mutex> lock(mtx);
cv.wait(lock, [] { return !buffer.empty(); });
// safe access to shared state
💡 Think of cv.wait(lock) as:
“Unlock-and-sleep, then wake-and-relock, all safely.”

This is why std::condition_variable::wait() requires a std::unique_lock<std::mutex>.

🎯 Summary
Concept                                                          Why
Lock required for cv.wait(lock)                 To protect shared data and do atomic sleep/unlock
What does wait() do?                            Unlocks mutex, sleeps; on wake, re-locks mutex
What if no lock?                                Race conditions, missed notifications, undefined behavior

*/