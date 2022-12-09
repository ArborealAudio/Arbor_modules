/**
 * Thread.h
 * Simple class for worker thread
 * Takes function pointers and sleeps otherwise
 */
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <queue>
#pragma once

// A class that represents a worker thread
class worker_thread
{
public:
    // Construct a worker thread
    worker_thread()
        : running(false), thread(&worker_thread::run)
    {
    }

    // Destruct a worker thread
    ~worker_thread()
    {
        // Stop the thread and wait for it to finish
        stop();
        thread.join();
    }

    // Stop the worker thread
    void stop()
    {
        // Set the running flag to false
        running = false;

        // Notify the worker thread that it should stop
        not_empty.notify_one();
    }

    // Add a function to the queue of functions to execute
    void add_work(std::function<void()> work)
    {
        // Lock the mutex
        std::unique_lock<std::mutex> lock(mutex);

        // Add the function to the queue
        work_queue.push(work);

        // Notify the worker thread that there is work to do
        not_empty.notify_one();
    }

private:
    // The worker thread function
    void run()
    {
        // Set the running flag to true
        running = true;

        // Execute functions from the queue until the thread is stopped
        while (running)
        {
            // Lock the mutex
            std::unique_lock<std::mutex> lock(mutex);

            // Wait until there is work to do
            not_empty.wait(lock, [this]
                           { return !running || !work_queue.empty(); });

            // If the thread is not running, stop processing functions
            if (!running)
                break;

            // Get the next function to execute
            auto work = work_queue.front();
            work_queue.pop();

            // Unlock the mutex
            lock.unlock();

            // Execute the function
            work();
        }
    }

    // A flag that indicates whether the thread is running
    std::atomic<bool> running;

    // The underlying thread
    std::thread thread;

    // The queue of functions to execute
    std::queue<std::function<void()>> work_queue;

    // The mutex that controls access to the queue of functions
    std::mutex mutex;

    // The condition variable that is used to notify the worker thread
    // when there is work to do
    std::condition_variable not_empty;
};
