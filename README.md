# High Performance Thread Pool

A reusable, scalable thread pool implemented in modern C++ designed for concurrent task execution. This project minimizes the overhead of thread creation and destruction by maintaining a persistent pool of worker threads, optimizing CPU utilization for both compute-bound and I/O-bound workloads.

## Features

* **Generic Task Submission:** Utilizes variadic templates and `std::packaged_task` to accept any callable object (functions, lambdas, functors) with arbitrary arguments.
* **Asynchronous Futures:** The `enqueue` interface returns a `std::future`, allowing the main thread to remain unblocked and retrieve results only when needed.
* **Lock-Based Task Scheduler:** Implements a shared work queue protected by `std::mutex` and RAII principles (`std::unique_lock`, `std::lock_guard`) to prevent race conditions and deadlocks.
* **Minimized Idle CPU Time:** Uses `std::condition_variable` to suspend idle worker threads, eliminating busy-waiting and reducing synchronization overhead.
* **Clean Resource Management:** Safely joins all threads upon destruction, ensuring no dangling threads or memory leaks occur during application shutdown.

## Benchmarking & Performance

The pool was benchmarked on an 8-core ARM64 processor (Apple Silicon) by stress-testing it with 20,000 concurrent compute-bound tasks.

* **Throughput:** Achieved a peak processing capacity of **~83,000 tasks per second**.
* **Execution Speedup:** Reduced total task execution time by **~5x (from 1.20s to 0.24s)** compared to a single-threaded sequential baseline.
* **Efficiency:** Validated efficient thread allocation and minimal lock contention by keeping scheduling overhead in the sub-millisecond range.

## Prerequisites

* A C++11 (or higher) compliant compiler.
* Support for POSIX threads (native on Linux and macOS).

## Getting Started (macOS / Linux)

1. **Clone the repository** (if applicable) or save the source code to `main.cpp`.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

2. **Compile the code** 
Using Apple Clang (macOS) or GCC (Linux), ensure you link the pthread library:
```bash
clang++ -std=c++11 -pthread main.cpp -o threadpool
