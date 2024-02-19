#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

#include "Logging.h"

#pragma once

class SingleThreadCtrl {
 public:
    using thread_function = std::function<void(void)>;

    SingleThreadCtrl(thread_function other) {
        setThreadFunction(other);
    }
    SingleThreadCtrl() = default;

    /*
     * useConditionVariable - Enable and set up std::condition_variable usage
     */
    void useConditionVariable() {
        using_cv = true;
    }

    /*
     * setFunction - Assign a thread function and starts the thread
     *
     * @param fn thread function with void(*)() signature
     */
    void setThreadFunction(thread_function fn) {
        if (!threadP)
            threadP = std::thread(fn);
        else {
            LOG_W("Function is already set in this instance");
        }
    }
    virtual void stop() {
        std::call_once(once, [this]{
            kRun = false;
            if (using_cv) {
                std::unique_lock<std::mutex> _(m);
                cv.notify_one();
            }
            if (threadP && threadP->joinable())
                threadP->join();
        });
    }

    virtual ~SingleThreadCtrl() {
        stop();
    }
 protected:
    std::atomic_bool kRun = true;
    std::condition_variable cv;
 private:
    std::optional<std::thread> threadP;
    std::once_flag once;
    // Used by std::cv
    bool using_cv = false;
    std::mutex m;
};