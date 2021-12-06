cmake_minimum_required (VERSION 3.2)

project(LoopIntensiveOperationsInC VERSION 0.25 DESCRIPTION "Operations with computing intensive arrays" LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-O3 -g -static")
set(CMAKE_C_FLAGS_RELEASE "-O3 -g -static")

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-O3 -g -static")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -static")

# Common infrastructure for kernels to manipulate their arrays.
add_executable(LoopIntensiveOperationsInC LoopIntensiveOperationsInC.c computationKernels.c)

# Tests
add_executable(testAllKernels tests/testAllKernels.c computationKernels.c)

# Link against math lib.
target_link_libraries(LoopIntensiveOperationsInC m)
target_link_libraries(testAllKernels m)

AUX_SOURCE_DIRECTORY(kernels KERNELS) 

# Separate implementation of all kernels.
#add_executable(LoopIntensiveOperationsInC LoopIntensiveOperationsInC.c ${KERNELS})

# Just some tests.
#add_executable(test test.c)