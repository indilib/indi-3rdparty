#pragma once

#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

namespace Temporary
{

// #PS: TODO move to INDI Library
class SingleWorker
{
public:
    SingleWorker()
    { }

    ~SingleWorker()
    { quit(); }

public:
    void run(const std::function<void(const std::atomic_bool &isAboutToQuit)> &function)
    {
        std::lock_guard<std::mutex> lock(runLock);
        mIsAboutToQuit = true;
        if (thread.joinable())
            thread.join();
        mIsAboutToQuit = false;
        mIsRunning = true;
        thread = std::thread([function, this]{
            function(this->mIsAboutToQuit);
            mIsRunning = false;
        });
    }
public:
    bool isAboutToQuit() const
    {
        return mIsAboutToQuit;
    }

    bool isRunning() const
    {
        return mIsRunning;
    }

public:
    void quit()
    {
        std::lock_guard<std::mutex> lock(runLock);
        mIsAboutToQuit = true;
    }

    void wait()
    {
        std::lock_guard<std::mutex> lock(runLock);
        if (thread.joinable())
            thread.join();
    }

private:
    std::atomic_bool mIsAboutToQuit {true};
    std::atomic_bool mIsRunning {false};
    std::mutex  runLock;
    std::thread thread;
};

}
