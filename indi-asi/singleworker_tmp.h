/*
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
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
    { return mIsAboutToQuit; }

    bool isRunning() const
    { return mIsRunning; }

public:
    void quit()
    {
        std::lock_guard<std::mutex> lock(runLock);
        mIsAboutToQuit = true;
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
