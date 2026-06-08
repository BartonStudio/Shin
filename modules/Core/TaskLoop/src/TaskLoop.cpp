#include "TaskLoop.hpp"
#include <Log.hpp>
#include <Module.hpp>

namespace Shin {
namespace Core {

    TaskLoop& TaskLoop::GetInstance() {
        static TaskLoop instance;
        return instance;
    }

    TaskLoop::~TaskLoop() {
        Shutdown();
    }

    void TaskLoop::Initialize(size_t threadCount) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_workers.empty()) return;

        if (threadCount == 0) {
            threadCount = std::thread::hardware_concurrency();
            if (threadCount == 0) threadCount = 2; // Fallback
        }

        m_stop = false;
        LOGI("TaskLoop") << "Initializing TaskLoop with " << threadCount << " threads.";

        for (size_t i = 0; i < threadCount; ++i) {
            m_workers.emplace_back(&TaskLoop::WorkerThread, this);
        }
    }

    void TaskLoop::Shutdown() {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_stop) return;
            m_stop = true;
        }

        m_condition.notify_all();

        for (std::thread& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_workers.clear();
            // Clear remaining tasks
            std::queue<std::function<void()>> empty;
            std::swap(m_tasks, empty);
        }
        
        LOGI("TaskLoop") << "TaskLoop shut down.";
    }

    void TaskLoop::PostTask(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_stop) {
                LOGW("TaskLoop") << "Discarding task: TaskLoop is shutting down.";
                return;
            }
            m_tasks.push(std::move(task));
        }
        m_condition.notify_one();
    }

    void TaskLoop::WorkerThread() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_condition.wait(lock, [this] {
                    return m_stop || !m_tasks.empty();
                });

                if (m_stop && m_tasks.empty()) {
                    return;
                }

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }

            try {
                if (task) {
                    task();
                }
            } catch (const std::exception& e) {
                LOGE("TaskLoop") << "Exception in background task: " << e.what();
            } catch (...) {
                LOGE("TaskLoop") << "Unknown exception in background task.";
            }
        }
    }

} // namespace Core

    /**
     * @brief Module wrapper for TaskLoop to integrate with Shin Engine lifecycle.
     */
    class TaskLoopModule : public IModule {
    public:
        std::string GetModuleName() const override { return "TaskLoop"; }

        bool OnInitialize(Core::Config& config) override {
            std::string threadsStr = config.GetValue("task_loop_threads", "0");
            size_t threadCount = 0;
            try {
                threadCount = static_cast<size_t>(std::stoul(threadsStr));
            } catch (...) {}
            
            Core::TaskLoop::GetInstance().Initialize(threadCount);
            return true;
        }

        void OnShutdown() override {
            Core::TaskLoop::GetInstance().Shutdown();
        }
    };

    SHIN_REGISTER_MODULE(TaskLoopModule)

} // namespace Shin
