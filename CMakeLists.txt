cmake_minimum_required(VERSION 3.5.0)
project(cpp_base VERSION 0.1.0 LANGUAGES C CXX)

add_executable(cpp_base main.cc)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
