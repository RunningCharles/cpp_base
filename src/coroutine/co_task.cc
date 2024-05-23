#include <iostream>
#include <list>
#include <thread>
#include <coroutine>
#include <exception>
#include "./co_task.h"

namespace co {
namespace task {

template <typename R>
struct TaskPromise;

/**
 * 协程任务结果，描述 Task 正常返回的结果和抛出的异常，需定义一个持有二者的类型
*/
template <typename T>
struct TaskResult {
  // 初始化为默认值
  explicit TaskResult() = default;

  // 当 Task 正常返回时用结果初始化 Result
  explicit TaskResult(T &&t) : _value(std::move(t)) {}

  // 当 Task 抛异常时用异常初始化 Result
  explicit TaskResult(std::exception_ptr &&ptr) : _exception_ptr(ptr) {}

  // 读取结果，有异常则抛出异常
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

/**
 * 协程任务，定义比较简单，能力多都是通过 promise_type 来实现的
*/
template <typename R>
struct Task {
  // 声明 promise_type 为 TaskPromise 类型
  using promise_type = TaskPromise<R>;

  constexpr bool await_ready() const noexcept {
    std::cout << "[" << &(handle.promise()) << "]" << "task await ready" << std::endl;
    return false;
  }

  // 当 task 执行完之后调用 resume
  void await_suspend(std::coroutine_handle<promise_type> handle) noexcept {
    std::cout << "[" << &(handle.promise()) << "]" << "task await suspend" << std::endl;
    finally([handle, this]() {
      std::cout << "[" << &(handle.promise()) << "]" << "task await suspend finally" << std::endl;
      handle.resume();
    });
  }

  // 协程恢复执行时，被等待的 Task 已经执行完，调用 get_result 来获取结果
  R await_resume() noexcept {
    std::cout << "[" << &(handle.promise()) << "]" << "task await resume" << std::endl;
    return get_result();
  }

  R get_result() {
    std::cout << "[" << &(handle.promise()) << "]" << "task get result" << std::endl;
    return handle.promise().get_result();
  }

  Task &then(std::function<void(R)> &&func) {
    std::cout << "[" << &(handle.promise()) << "]" << "task then" << std::endl;
    handle.promise().on_completed([func, this](auto result) {
      std::cout << "[" << &(handle.promise()) << "]" << "task then on completed" << std::endl;
      try {
        func(result.get_or_throw());
      } catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
      }
    });
    return *this;
  }

  Task &catching(std::function<void(std::exception &)> && func) {
    std::cout << "[" << &(handle.promise()) << "]" << "task catching" << std::endl;
    handle.promise().on_completed([func, this](auto result) {
      std::cout << "[" << &(handle.promise()) << "]" << "task catching on completed" << std::endl;
      try {
        result.get_or_throw();
      } catch(std::exception& e) {
        func(e);
      }
    });
    return *this;
  }

  Task &finally(std::function<void()> &&func) {
    std::cout << "[" << &(handle.promise()) << "]" << "task finally" << std::endl;
    handle.promise().on_completed([func, this](auto result) {
      std::cout << "[" << &(handle.promise()) << "]" << "task finally on completed" << std::endl;
      func();
    });
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

public:
  std::coroutine_handle<promise_type> handle;
};

/**
 * promise_type 是连接协程内外的桥梁，想要拿到什么，找 promise_type 要
 * promise_type 可通过 std::coroutine_handle 的 promise 获取
 * promise_type 可通过 std::coroutine_handle 的 from_promise 转化为 std::coroutine_handle
*/
template <typename R>
struct TaskPromise {
  // 协程立即执行，不进行挂起
  std::suspend_never initial_suspend() {
    std::cout << "[" << this << "]" << "task initial suspend" << std::endl;
    return {};
  }

  // 执行结束后挂起，等待外部（task.handle.destroy()）销毁
  std::suspend_always final_suspend() noexcept {
    std::cout << "[" << this << "]" << "task final suspend" << std::endl;
    return {};
  }

  // 构造协程的返回值对象 Task
  Task<R> get_return_object() {
    std::cout << "[" << this << "]" << "task get return object" << std::endl;
    return Task{ std::coroutine_handle<TaskPromise>::from_promise(*this) };
  }

  // 将异常存入 result
  void unhandled_exception() {
    std::cout << "[" << this << "]" << "task unhandled exception" << std::endl;
    std::lock_guard lock(completion_lock);
    result = TaskResult<R>(std::current_exception());
    // 通知 get_result 当中的 wait
    completion.notify_all();
    // 调用回调
    notify_callbacks();
  }

  // 将返回值存入 result，对应于协程内部的 'co_return value'
  void return_value(R value) {
    std::cout << "[" << this << "]" << "task return value" << std::endl;
    std::lock_guard lock(completion_lock);
    result = TaskResult<R>(std::move(value));
    // 通知 get_result 当中的 wait
    completion.notify_all();
    // 调用回调
    notify_callbacks();
  }

  R get_result() {
    std::cout << "[" << this << "]" << "task get result" << std::endl;
    std::unique_lock lock(completion_lock);
    if (!result.has_value()) {
      // 如果 result 没有值，说明协程还没有运行完，等待值被写入再返回
      // 等待写入值之后调用 notify_all
      completion.wait(lock);
    }
    // 如果有值，则直接返回（或者抛出异常）
    return result->get_or_throw();
  }

  void on_completed(std::function<void(TaskResult<R>)> &&func) {
    std::cout << "[" << this << "]" << "task on completed" << std::endl;
    std::unique_lock lock(completion_lock);
    if (result.has_value()) { // result 已经有值
      auto value = result.value();
      lock.unlock(); // 解锁之后再调用 func
      func(value);
    } else { // 否则添加回调函数，等待调用
      completion_callbacks.push_back(func);
    }
  }

private:
  void notify_callbacks() {
    auto value = result.value();
    for (auto &callback : completion_callbacks) {
      callback(value);
    }
    completion_callbacks.clear(); // 调用完成，清空回调
  }

private:
  // 使用 std::optional 可以区分协程是否执行完成
  std::optional<TaskResult<R>> result;

  std::mutex completion_lock;
  std::condition_variable completion;

  // 回调列表，我们允许对同一个 Task 添加多个回调
  std::list<std::function<void(TaskResult<R>)>> completion_callbacks;
};

Task<int> simple_task2() {
  std::cout << "begin simple task 2" << std::endl;
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(1s);
  std::cout << "end simple task 2 after 1s" << std::endl;
  co_return 2;
}

Task<int> simple_task3() {
  std::cout << "begin simple task 3" << std::endl;
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2s);
  std::cout << "end simple task 3 after 2s" << std::endl;
  co_return 3;
}

Task<int> simple_task() {
  std::cout << "begin simple task" << std::endl;
  auto result2 = co_await simple_task2();
  auto result3 = co_await simple_task3();
  std::cout << "end simple task" << std::endl;
  co_return 1 + result2 + result3;
}

void Run() {
  std::cout << "start run task" << std::endl;
  {
    auto task = simple_task();
    std::cout << "[" << &task.handle.promise() << "]" << "run simple task" << std::endl;
    task.then([](int i) {
      std::cout << "run simple task end, ret: " << i << std::endl;
    }).catching([](std::exception &e) {
      std::cerr << "run simple task failed, exception: " << e.what() << std::endl;
    }).finally([]() {
      std::cout << "run simple task finally" << std::endl;
    });;
    try {
      auto i = task.get_result();
      std::cout << "get task result, ret: " << i << std::endl;
    } catch (std::exception &e) {
      std::cerr << "get task result failed, exception: " << e.what() << std::endl;
    }
  }
  std::cout << "end run task" << std::endl;
}

} // namespace task
} // naemspace co
