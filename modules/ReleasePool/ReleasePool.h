// ReleasePool.h

/***
BEGIN_JUCE_MODULE_DECLARATION
ID: ReleasePool
vendor: Arboreal Audio
version: 1.0
name: ReleasePool
description: Class for managing safe deletion of objects which need realtime capability
dependencies: juce_core
END_JUCE_MODULE_DECLARATION
***/

#pragma once
#include <juce_core/juce_core.h>

namespace strix
{

class ReleasePoolShared : Timer
{
    std::vector<std::shared_ptr<void>> pool;
    std::mutex m;

    void timerCallback() override
    {
        std::lock_guard<std::mutex> lock(m);
        pool.erase(
            std::remove_if(
                pool.begin(), pool.end(),
                [](auto &object)
                { return object.use_count() <= 1; }),
            pool.end());
    }
public:
    ReleasePoolShared() { startTimer(1000); }

    template <typename obj>
    void add(const std::shared_ptr<obj>& object)
    {
        if (object == nullptr)
            return;

        std::lock_guard<std::mutex> lock(m);
        pool.emplace_back(object);
    }
};

} //namespace strix