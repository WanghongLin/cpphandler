cmake_minimum_required(VERSION 3.10)
project(cpphandler)

set(CMAKE_CXX_STANDARD 11)

add_executable(TestRemove TestRemove.cpp Handler.h)
add_executable(TestCallbackLambda TestCallbackLambda.cpp Handler.h)
add_executable(TestCallbackFunction TestCallbackFunction.cpp Handler.h)
add_executable(TestCallbackFunctor TestCallbackFunctor.cpp Handler.h)

enable_testing()
add_test(TestRemove TestRemove)
add_test(TestCallbackLambda TestCallbackLambda)
add_test(TestCallbackFunction TestCallbackFunction)
add_test(TestCallbackFunctor TestCallbackFunctor)
