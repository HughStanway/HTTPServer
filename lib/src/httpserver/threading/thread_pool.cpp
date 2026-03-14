#include <httpserver/monitoring/log_event.h>
#include <httpserver/monitoring/logger.h>
#include <httpserver/threading/thread_pool.h>
#include <httpserver/utils/config.h>

namespace HTTPServer {

ThreadPool::ThreadPool(size_t thread_count) {
  d_workers.reserve(thread_count);
  for (size_t i = 0; i < thread_count; ++i) {
    d_workers.emplace_back([this, i] {
      std::string name = "http-worker-" + std::to_string(i);
#if defined(__APPLE__)
      pthread_setname_np(name.c_str());
#elif defined(__linux__)
      pthread_setname_np(pthread_self(), name.c_str());
#endif
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
  LOG_EVENT(LogLevel::INFO, LogEvent("all_client_threads_finished"));
}

int ThreadPool::enqueue(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(d_mtx);

    if (d_task_queue.size() >= Config::get().kMaxQueueSize) {
      LOG_EVENT(LogLevel::WARN, LogEvent("thread_pool_queue_limit_reached"));
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