#include <TaskLoop.hpp>
#include <Log.hpp>
#include <ShinCore.hpp>
#include <iostream>
#include <chrono>
#include <atomic>

int main() {
    // 1. Manually initialize log for testing
    Shin::Log::Init();
    
    Shin::LOGI("Test") << "Starting TaskLoop test...";

    // 2. TaskLoop is a module, but we can also use it directly for testing
    auto& loop = Shin::Core::TaskLoop::GetInstance();
    loop.Initialize(4);

    std::atomic<int> counter{0};
    const int totalTasks = 10;

    for (int i = 0; i < totalTasks; ++i) {
        loop.PostTask([i, &counter]() {
            Shin::LOGI("Test") << "Executing task " << i << " on thread " << std::this_thread::get_id();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            counter++;
        });
    }

    Shin::LOGI("Test") << "All tasks posted. Waiting for completion...";

    // Wait for tasks to complete (polling for simplicity in test)
    while (counter < totalTasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    Shin::LOGI("Test") << "All tasks finished. Counter: " << counter;

    loop.Shutdown();
    Shin::LOGI("Test") << "TaskLoop test finished.";

    return 0;
}
