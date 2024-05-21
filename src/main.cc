#include <iostream>
#include "./coroutine/co_generator.h"

int main(int argc, char *argv[]){
    co::generator::Run();
    std::cout << "Hello World!" << std::endl;
}
