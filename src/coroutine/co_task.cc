#include <iostream>
#include <coroutine>
#include <exception>
#include "./co_task.h"

namespace co {
namespace task {

template <typename R>
struct TaskPromise;

template <typename T>
struct TaskResult {
  explicit TaskResult() = default;
  explicit TaskResult(T &&t) : _value(std::move(t)) {}
  explicit TaskResult(std::exception_ptr &&ptr) : _exception_ptr(ptr) {}

  T get_or_throw() {
    if (_exception_ptr) {
      std::rethrow_exception(_exception_ptr);
    }
    return _value;
  }

private:
  T _value{};
  std::exception_ptr _exception_ptr;
};

template <typename R>
struct Task {
  using promise_type = TaskPromise<R>;

  R get_result() {
    return handle.promise().get_result();
  }

  Task &then(std::function<void(R)> &&func) {
    handle.promise().on_completed([func](auto result) {
      try {
        func(result.get_or_throw());
      } catch(std::exception& e) {
        std::cerr << e.what() << '\n';
      }
    });
    return *this;
  }

  Task &catching(std::function<void(std::exception &)> && func) {
    handle.promise().on_completed([func](auto result) {
      try {
        result.get_or_throw();
      } catch(std::exception& e) {
        func(e);
      }
    });
    return *this;
  }

  Task &finally(std::function<void()> &&func) {
    handle.promise().on_completed([func](auto result) { func(); } );
    return *this;
  }

  explicit Task(std::coroutine_handle<promise_type> handle) noexcept: handle(handle) {}
  explicit Task(Task &&task) noexcept: handle(std::exchange(task.handle, {})) {}
  Task(Task &) = delete;
  Task &operator=(Task &) = delete;
  ~Task() {
    if (handle) {
      handle.destroy();
    }
  }

private:
  std::coroutine_handle<promise_type> handle;
};

template <typename R>
struct TaskAwaiter {
  constexpr bool await_ready() const noexcept {
    return false;
  }

  void await_suspend(std::coroutine_handle<> handle) noexcept {
    task.finally([handle]() {
      handle.resume();
    });
  }

  R await_resume() noexcept {
    return task.get_result();
  }

  explicit TaskAwaiter(Task<R> &&task) noexcept : task(std::move(task)) {}
  explicit TaskAwaiter(TaskAwaiter &&completion) noexcept : task(std::exchange(completion.task, {})) {}

  TaskAwaiter(TaskAwaiter &) = delete;
  TaskAwaiter &operator=(TaskAwaiter &) = delete;

private:
  Task<R> task;
};

template <typename R>
struct TaskPromise {
  std::suspend_never initial_suspend() {
    std::cout << "task initial suspend" << std::endl;
    return {};
  }

  std::suspend_always final_suspend() noexcept {
    std::cout << "task final suspend" << std::endl;
    return {};
  }

  Task<R> get_return_object() {
    return Task{ std::coroutine_handle<TaskPromise>::from_promise(*this) };
  }

  void unhandled_exception() {
    result = TaskResult<R>(std::current_exception());
  }

  void return_value(R value) {
    result = TaskResult<R>(std::move(value));
  }

  template <typename _R>
  TaskAwaiter<_R> await_tranform(Task<_R> &&task) {
    return TaskAwaiter<_R>(std::move(task));
  }

private:
  std::optional<TaskResult<R>> result;
};

void Run() {
  std::cout << "start run task" << std::endl;
  {
  }
  std::cout << "end run task" << std::endl;
}

} // namespace task
} // naemspace co
