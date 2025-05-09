#ifndef THREADPOOL
#define THREADPOOL

#include <condition_variable>
#include <functional>
#include <future>
#include <tuple>
#include <mutex>
#include <queue>
#include <thread>

// Useful shortcut, equivalent to std::forward<decltype(...)>(...)
#define FWD(...) static_cast<decltype(__VA_ARGS__) &&>(__VA_ARGS__)

struct threadpool {
  explicit threadpool(std::size_t n)
  {
    for (std::size_t i = 0; i < n; ++i) {
      threads_.emplace_back(&threadpool::run, this);
    }
  }

  ~threadpool()
  {
    stop();
    join();
  }

  void stop()
  {
    {
      std::unique_lock g(mutex_);
      if (stopped_)
        return;
      stopped_ = true;
      cv_.notify_all();
    }
  }

  void join()
  {
    // No need to call join(), std::jthread knows to do it
    threads_.clear();
  }

  auto submit(auto &&f, auto &&...args) -> bool
  {
    using result_t = std::invoke_result_t<decltype(f), std::remove_cvref_t<decltype(args)>&&...>;
    std::packaged_task<result_t(decltype(args)...)> inner_task(FWD(f));
    auto future = inner_task.get_future();

    std::lock_guard g(mutex_);
    if (stopped_)
      return false;

    queue_.emplace(                           //
        ([inner_task = std::move(inner_task), //
          args = std::tuple<std::remove_cvref_t<decltype(args)>...>(FWD(args)...)]() mutable {
          std::apply(inner_task, std::move(args));
        }));
    cv_.notify_one();
    return true;
  }

private:
  void run()
  {
    while (true) {
      std::unique_lock g(mutex_);
      if (queue_.empty())
        cv_.wait(g, [&, this] { return !queue_.empty() || stopped_; });

      // Do not leak any task, even when stopped
      if (stopped_ && queue_.empty())
        return;

      if (queue_.empty())
        continue;

      auto t = std::move(queue_.front());
      queue_.pop();
      g.unlock();

      t();
    }
  }

  using task = std::move_only_function<void()>;

  std::vector<std::jthread> threads_;
  std::queue<task> queue_;
  bool stopped_ = false;
  std::mutex mutex_;
  std::condition_variable cv_;
};

#endif // THREADPOOL
