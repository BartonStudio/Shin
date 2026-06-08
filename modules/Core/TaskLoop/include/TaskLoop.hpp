#pragma once
#include <functional>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

#ifdef _WIN32
    #ifdef SHIN_CORE_EXPORTS
        #define SHIN_API __declspec(dllexport)
    #else
        #define SHIN_API __declspec(dllimport)
    #endif
#else
    #define SHIN_API __attribute__((visibility("default")))
#endif

namespace Shin {
namespace Core {

    /**
     * @brief A singleton task dispatcher that executes tasks on a background thread pool.
     */
    class SHIN_API TaskLoop {
    public:
        static TaskLoop& GetInstance();

        /**
         * @brief Initialize the task loop with a specified number of threads.
         * @param threadCount Number of threads. If 0, uses hardware concurrency.
         */
        void Initialize(size_t threadCount = 0);

        /**
         * @brief Stop the task loop and join all threads.
         */
        void Shutdown();

        /**
         * @brief Post a task to be executed on a background thread.
         */
        void PostTask(std::function<void()> task);

    private:
        TaskLoop() = default;
        ~TaskLoop();
        TaskLoop(const TaskLoop&) = delete;
        TaskLoop& operator=(const TaskLoop&) = delete;

        void WorkerThread();

        std::vector<std::thread> m_workers;
        std::queue<std::function<void()>> m_tasks;

        std::mutex m_queueMutex;
        std::condition_variable m_condition;
        std::atomic<bool> m_stop{false};
    };

} // namespace Core
} // namespace Shin
