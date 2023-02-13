# Almost all CMake files should start with this
# You should always specify a range with the newest
# and oldest tested versions of CMake. This will ensure
# you pick up the best policies.
cmake_minimum_required(VERSION 3.1...3.25)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/tests/simpleBinExamples/testIf/build")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O3 -static")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -static")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3")

# This is your project statement. You should always list languages;
# Listing the version is nice here since it sets lots of useful variables
project(
  TestIf
  VERSION 1.0
  LANGUAGES C)

# If you set any CMAKE_ variables, that can go here.
# (But usually don't do this, except maybe for C++ standard)

# Find packages go here.

# You should usually split this into folders, but this is a simple example

# This is a "default" library, and will match the *** variable setting.
# Other common choices are STATIC, SHARED, and MODULE
# Including header files here helps IDEs but is not required.
# Output libname matches target name, with the usual extensions on your system
#add_library(MyLibExample simple_lib.cpp simple_lib.hpp)

# Link each target with other targets or add options, etc.

# Adding something we can run - Output name matches target name
add_executable(testIf testIf.c)

# Make sure you link your targets with this command. It can also link libraries and
# even flags, so linking a target that does not exist will not give a configure-time error.
#target_link_libraries(MyExample PRIVATE MyLibExample)
