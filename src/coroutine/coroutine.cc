#include<iostream>
#include "coroutine.h"

namespace CO {

void Fun() {
  std::cout << 1 << std::endl;
  std::cout << 2 << std::endl;
  std::cout << 3 << std::endl;
  std::cout << 4 << std::endl;
}

void Test() {
  Fun();
}

} // end namespace CO
