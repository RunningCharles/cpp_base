#include<iostream>
#include<coroutine>
#include "generator.h"

namespace co {
namespace generator {

template <typename T>
struct Generator {

  // 协程执行完成之后，外部读取值时抛出的异常
  class ExhausteException: std::exception {};

  struct promise_type {
    T value;
    bool is_ready = false;

    // 开始执行时直接挂起等待外部调用 resume 获取下一个值
    std::suspend_always initial_suspend() {
      std::cout << "generator initial suspend" << std::endl;
      return {};
    }
    
    // 执行结束后不需要挂起
    // std::suspend_never final_suspend() noexcept {
    //   std::cout << "generator final suspend" << std::endl;
    //   return {};
    // }

    // 总是挂起，让 Generator 来销毁
    std::suspend_always final_suspend() noexcept {
      std::cout << "generator final suspend" << std::endl;
      return {};
    }

    // 不会抛出异常，这里不做任何处理
    void unhandled_exception() {
      std::cout << "generator unhandled exception" << std::endl;
    }
    
    // 构造协程的返回值类型，绑定 handle
    Generator get_return_object() {
      std::cout << "generator get return object" << std::endl;
      return Generator{ std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    // 没有返回值
    void return_void() {
      std::cout << "generator return void" << std::endl;
    }
    
    // 将基本类型转化为 awaiter
    // std::suspend_always await_transform(T value) {
    //   std::cout << "generator await transform: " << this->value << " to " << value << std::endl;
    //   this->value = value;
    //   is_ready = true;
    //   return {};
    // }

    // 将 await_transform 替换为 yield_value，对应 co_await 调整为 co_yield
    // co_yield expr 等价于 co_await promise.yield_value(expr)
    std::suspend_always yield_value(T value) {
      std::cout << "generator yield value: " << this->value << " to " << value << std::endl;
      this->value = value;
      is_ready = true;
      return {};
    }
  };

  std::coroutine_handle<promise_type> handle;

  // 显示构造函数，禁止隐式转换
  explicit Generator(std::coroutine_handle<promise_type> handle) noexcept
    : handle(handle) {}

  
  // 对于每一个协程实例，都有且仅能有一个 Generator 实例与之对应，因此只支持移动对象，而不支持复制对象。
  Generator(Generator &&generator) noexcept
    : handle(std::exchange(generator.handle, {})) {}
  Generator(Generator &) = delete;
  Generator &operator=(Generator &) = delete;

  ~Generator() {
    std::cout << "generator destroy" << std::endl;
    // 销毁协程
    handle.destroy();
  }

  bool has_next() {
    // 协程已经执行完成
    if (handle.done()) {
      std::cout << "generator has next, done(1)" << std::endl;
      return false;
    }

    // 协程还没有执行完成，并且下一个值还没有准备好
    if (!handle.promise().is_ready) {
      std::cout << "generator has next, hasn't done, not ready" << std::endl;
      handle.resume();
    }
    
    if (handle.done()) {
      // 恢复执行之后协程执行完，这时候必然没有通过 co_await 传出值来
      std::cout << "generator has next, done(2)" << std::endl;
      return false;
    } else {
      std::cout << "generator has next, hasn't done" << std::endl;
      return true;
    }
  }

  T next() {
    if (has_next()) {
      // 此时一定有值，is_ready 为 true 
      // 消费当前的值，重置 is_ready 为 false
      handle.promise().is_ready = false;
      return handle.promise().value;
    }

    throw ExhausteException();
  }

  // 使用 C++ 17 的折叠表达式（fold expression）的特性
  template<typename ...TArgs>
  Generator static from(TArgs ...args) {
    (co_yield args, ...);
  }
};

Generator<int32_t> sequence() {
  for (int32_t i = 0; i < 10; i++) {
    // 使用 co_await 更多的关注点在挂起自己，等待别人上，而使用 co_yield 则是挂起自己传值出去
    // co_await i; // await_transform
    co_yield i; // yield_value
  }
}

void Run() {
  // auto gen = sequence();
  auto gen = Generator<int32_t>::from(5, 4, 3, 2, 1);
  for (int32_t i = 0; i < 15; i++) {
    if (gen.has_next()) {
      std::cout << gen.next() << std::endl;
    } else {
      break;
    }
  }
}

} // end namespace generator
} // end namespace CO
