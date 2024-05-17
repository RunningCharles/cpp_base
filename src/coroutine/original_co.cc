#include<iostream>
#include<coroutine>
#include "original_co.h"

namespace co {
namespace original {

struct Generator {
  struct promise_type {
    uint32_t value;

    std::suspend_always initial_suspend() {
      std::cout << "generator initial suspend" << std::endl;
      return {};
    }
    
    std::suspend_never final_suspend() noexcept {
      std::cout << "generator final suspend" << std::endl;
      return {};
    }

    void unhandled_exception() {
      std::cout << "generator unhandled exception" << std::endl;
    }

    Generator get_return_object() {
      std::cout << "generator get return object" << std::endl;
      return Generator{ std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_void() {
      std::cout << "generator return void" << std::endl;
    }

    std::suspend_always await_transform(uint32_t value) {
      std::cout << "generator await transform: " << this->value << " to " << value << std::endl;
      this->value = value;
      return {};
    }
  };

  std::coroutine_handle<promise_type> handle;

  uint32_t next() {
    handle.resume();
    return handle.promise().value;
  }
};

Generator sequence() {
  for (uint32_t i = 0; i < 10; i++) {
    co_await i;
  }
}

void RunGenerator() {
  auto gen = sequence();
  for (uint32_t i = 0; i < 15; i++) {
    std::cout << gen.next() << std::endl;
  }
}

void Test() {
  RunGenerator();
}

} // end namespace original
} // end namespace CO
