/**
 * LiteThread.h
 * A small job thread for one-off async tasks
 * Ideally you'd init this before the objects/methods you want to be async,
 * so that the overhead of creating a thread is not taking place just prior to
 * the invocation of the work you want to do. Hope that makes sense.
*/

#pragma once
#include <JuceHeader.h>

struct LiteThread : Thread
{
    /** @param maxNumJobs max number of jobs to be run. Thread will be destroyed once this limit is reached. -1 means no limit */
    LiteThread(int maxNumJobs) : Thread("LiteThread"), jobLimit(maxNumJobs)
    {
        DBG("Starting check thread...");
        startThread(Thread::Priority::background);
    }

    ~LiteThread() override
    {
        if (isThreadRunning())
            stopThread(-1);
        DBG("Stopping check thread...");
        stopThread(1000);
    }

    void run() override
    {
        working = true;
        while (!threadShouldExit() && working)
        {
            if (!jobs.empty())
            {
                auto job = jobs.front();
                jobs.pop();
                job();
                jobCount += 1;
                if (jobCount >= jobLimit && jobLimit > -1)
                    working = false;
            }
            else if (jobCount >= jobLimit && jobLimit > -1)
                working = false;
            else
                wait(100);
        }
        DBG("Check thread exited loop");
    }

    /* locking method for adding jobs to the thread */
    void addJob(std::function<void()> job)
    {
        std::unique_lock<std::mutex> lock(mutex);
        jobs.emplace(job);
        notify();
    }

    bool working = true;

private:
    std::mutex mutex;
    std::queue<std::function<void()>> jobs;
    int jobCount = 0;
    int jobLimit = -1; // if you have foreknowledge of how many jobs you need
};