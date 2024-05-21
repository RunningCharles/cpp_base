#include "./coroutine/co_generator.h"
#include "./coroutine/co_task.h"

int main(int argc, char *argv[]){
  co::generator::Run();
  co::task::Run();
}
