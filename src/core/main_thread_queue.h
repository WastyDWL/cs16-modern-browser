// Tasks posted from CEF threads, run on the game's main thread from RunFrame.
#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace nc {

class MainThreadQueue {
public:
    void post(std::function<void()> task);
    // Runs the tasks queued before this call; tasks they post run on the next drain.
    void drain();

private:
    std::mutex mutex_;
    std::vector<std::function<void()>> tasks_;
};

}  // namespace nc
