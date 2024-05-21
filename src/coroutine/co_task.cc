#include <iostream>
#include <exception>
#include "./co_task.h"

namespace co {
namespace task {

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

void Run() {
  std::cout << "start run task" << std::endl;
  {
  }
  std::cout << "end run task" << std::endl;
}

} // namespace task
} // naemspace co
