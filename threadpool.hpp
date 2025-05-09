#ifndef THREADPOOL
#define THREADPOOL

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>

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
    join(); // similarly like std::jthread
  }

  void stop()
  {
    std::unique_lock guard(mutex_);
    if (stopped_) {
      return;
    }
    stopped_ = true;
    cv_.notify_all();
  }

  void join() { threads_.clear(); }

  auto submit(auto &&fun, auto &&...args) -> bool
  {
    using result_t = std::invoke_result_t<decltype(fun), std::remove_cvref_t<decltype(args)> &&...>;
    std::packaged_task<result_t(decltype(args)...)> inner_task(FWD(fun));
    auto future = inner_task.get_future();

    std::lock_guard guard(mutex_);
    if (stopped_) {
      return false;
    }

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
      std::unique_lock guard(mutex_);
      constexpr auto predicate = [](threadpool &self) -> bool { return !self.queue_.empty() || self.stopped_; };
      if (!predicate(*this)) {
        cv_.wait(guard, [predicate, this] { return predicate(*this); });
      }

      if (queue_.empty()) {
        if (stopped_) {
          return;
        }
        continue;
      }
      // else do not leak any task, even when stopped

      auto task = std::move(queue_.front());
      queue_.pop();
      guard.unlock();

      task();
    }
  }

  using task_t = std::move_only_function<void()>;

  std::vector<std::jthread> threads_;
  std::queue<task_t> queue_;
  bool stopped_ = false;
  std::mutex mutex_;
  std::condition_variable cv_;
};

#endif // THREADPOOL
