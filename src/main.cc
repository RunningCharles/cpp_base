#include <iostream>
#include "./coroutine/generator.h"

int main(int argc, char *argv[]){
    co::generator::Run();
    std::cout << "Hello World!" << std::endl;
}
