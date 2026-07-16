#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <chrono>

class ThreadPool {
public:
    // Constructor: Hire the organizers and put them in the room
    ThreadPool(size_t threads);
    
    // Enqueue: The generic universal envelope for tasks
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    
    // Destructor: Tell everyone to go home
    ~ThreadPool();

private:
    std::vector<std::thread> workers;           // The organizers
    std::queue<std::function<void()>> tasks;    // The whiteboard/task queue
    
    std::mutex queue_mutex;                     // The marker pen (prevents race conditions)
    std::condition_variable condition;          // The WhatsApp ping (wakes sleeping workers)
    bool stop;                                  // Flag to close up shop
};

// --- Implementation ---

ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for(size_t i = 0; i < threads; ++i)
        workers.emplace_back(
            [this] {
                for(;;) {
                    std::function<void()> task;
                    {
                        // unique_lock allows unlocking while sleeping, and re-locking on wake
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        
                        // Sleep UNTIL there is a task OR the pool is shutting down
                        this->condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); });
                        
                        if(this->stop && this->tasks.empty())
                            return;
                        
                        // Grab the task off the front of the queue
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    // Execute the task outside the lock so others can use the queue
                    task();
                }
            }
        );
}

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using return_type = decltype(f(args...));

    // Wrap the function in a packaged_task to link it to a future
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
            
        // Add the task to the queue
        tasks.emplace([task](){ (*task)(); });
    }
    // Ping one sleeping worker that a new task is ready
    condition.notify_one();
    return res;
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }
    // Wake up ALL workers so they can see the stop flag and exit
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

// --- Execution Example ---

// A mock I/O bound task (e.g., calling a sponsor and waiting)
std::string callSponsor(int id, int waitTimeMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTimeMs));
    return "Sponsor " + std::to_string(id) + " agreed to fund!";
}

// A mock Compute bound task (e.g., calculating budget)
int calculateBudget(int base, int multiplier) {
    return base * multiplier;
}

// int main() {
//     std::cout << "Starting Thread Pool with 4 workers...\n";
//     ThreadPool pool(4);

//     std::vector<std::future<std::string>> string_results;
//     std::vector<std::future<int>> int_results;

//     // Submit 5 I/O bound tasks
//     std::cout << "Submitting I/O tasks...\n";
//     for(int i = 1; i <= 5; ++i) {
//         string_results.emplace_back(
//             pool.enqueue(callSponsor, i, 500) // Sleep for 500ms
//         );
//     }

//     // Submit 5 Compute bound tasks
//     std::cout << "Submitting Compute tasks...\n";
//     for(int i = 1; i <= 5; ++i) {
//         int_results.emplace_back(
//             pool.enqueue(calculateBudget, i, 1000)
//         );
//     }

//     // Fetch and print results (this blocks until the futures are ready)
//     std::cout << "\n--- Fetching Results ---\n";
//     for(auto && result : string_results) {
//         std::cout << result.get() << "\n"; 
//     }
//     for(auto && result : int_results) {
//         std::cout << "Calculated budget: $" << result.get() << "\n";
//     }

//     std::cout << "All tasks completed. Shutting down pool.\n";
//     return 0;
// }




// A pure CPU-bound task: Calculate the sum of factors for a large number
long long heavyComputeTask(int n) {
    long long sum = 0;
    for (int i = 1; i <= n; ++i) {
        if (n % i == 0) sum += i;
    }
    return sum;
}

int main() {
    const int num_tasks = 20000; // Throw 20,000 heavy tasks at the pool
    const int workload_size = 50000;
    
    // --- 1. Single-Threaded Baseline (Sequential Execution) ---
    std::cout << "Running single-threaded baseline sequentially...\n";
    auto start_seq = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_tasks; ++i) {
        volatile long long res = heavyComputeTask(workload_size); 
    }
    auto end_seq = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_seq = end_seq - start_seq;
    std::cout << "Sequential Time: " << duration_seq.count() << " seconds\n\n";

    // --- 2. Thread Pool Parallel Execution ---
    // Change this number to test different pool sizes (e.g., 2, 4, 8, 16)
    unsigned int num_threads = std::thread::hardware_concurrency(); 
    std::cout << "Running with Thread Pool (" << num_threads << " worker threads)...\n";
    
    auto start_pool = std::chrono::high_resolution_clock::now();
    {
        ThreadPool pool(num_threads);
        std::vector<std::future<long long>> results;
        results.reserve(num_tasks);

        // Flood the queue with all 20,000 tasks instantly
        for (int i = 0; i < num_tasks; ++i) {
            results.emplace_back(pool.enqueue(heavyComputeTask, workload_size));
        }

        // Wait for all tasks to finish executing
        for (auto&& fut : results) {
            fut.get();
        }
    } // ThreadPool destructor is called here, ensuring clean cleanup
    
    auto end_pool = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_pool = end_pool - start_pool;

    // --- 3. Calculate the Metrics ---
    double speedup = duration_seq.count() / duration_pool.count();
    double throughput = num_tasks / duration_pool.count();

    std::cout << "-------------------------------------------\n";
    std::cout << "Thread Pool Time : " << duration_pool.count() << " seconds\n";
    std::cout << "Performance Speedup: " << speedup << "x faster\n";
    std::cout << "Throughput         : " << throughput << " tasks/second\n";
    std::cout << "-------------------------------------------\n";

    return 0;
}