#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace HTTPServer {

class ThreadPool {
 public:
  explicit ThreadPool(size_t thread_count);
  ~ThreadPool();
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  void stop();
  int enqueue(std::function<void()> task);

 private:
  static constexpr size_t kMaxQueueSize = 1024;

  std::vector<std::thread> d_workers;
  std::queue<std::function<void()>> d_task_queue;
  std::mutex d_mtx;
  std::condition_variable d_condition_variable;
  std::atomic<bool> d_running{true};

  void worker_loop();
};

}  // namespace HTTPServer

#endif