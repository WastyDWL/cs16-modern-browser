#include "main_thread_queue.h"

namespace nc {

void MainThreadQueue::post(std::function<void()> task) {
    std::lock_guard lock(mutex_);
    tasks_.push_back(std::move(task));
}

void MainThreadQueue::drain() {
    std::vector<std::function<void()>> batch;
    {
        std::lock_guard lock(mutex_);
        batch.swap(tasks_);
    }
    for (auto& t : batch) t();
}

}  // namespace nc
