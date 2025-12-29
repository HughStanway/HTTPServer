#include "httpserver/thread_pool.h"

#include "httpserver/logger.h"

namespace HTTPServer {

ThreadPool::ThreadPool(size_t thread_count) {
  d_workers.reserve(thread_count);
  for (size_t i = 0; i < thread_count; ++i) {
    d_workers.emplace_back([this, i] {
      pthread_setname_np(("http-worker-" + std::to_string(i)).c_str());
      worker_loop();
    });
  }
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::stop() {
  if (!d_running.exchange(false)) {
    return;
  }

  d_condition_variable.notify_all();
  for (auto& worker : d_workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  LOG_INFO("Shutdown: All client threads finished.");
}

int ThreadPool::enqueue(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(d_mtx);

    if (d_task_queue.size() >= kMaxQueueSize) {
      LOG_WARN("Thread pool queue limit reached - rejecting connection");
      return -1;
    }
    d_task_queue.push(std::move(task));
  }
  d_condition_variable.notify_one();
  return 0;
}

void ThreadPool::worker_loop() {
  while (d_running) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(d_mtx);
      d_condition_variable.wait(
          lock, [this]() { return !d_task_queue.empty() or !d_running; });

      if (!d_running && d_task_queue.empty()) {
        return;
      }

      task = std::move(d_task_queue.front());
      d_task_queue.pop();
    }
    task();
  }
}

}  // namespace HTTPServer