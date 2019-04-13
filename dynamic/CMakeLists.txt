set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g")

enable_language(C CXX)

set(DYN_SRC_DIR "${PROJECT_SOURCE_DIR}/dynamic")
#find DynamoRIO library
#set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/../dynamorio/build/cmake")
if (JANUS_X86_SUPPORT)
set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/external/DynamoRIO-Linux-7.0.0-RC1/cmake")
elseif(JANUS_ARM64_SUPPORT)
set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/external/DynamoRIO-AArch64-Linux-7.0.0-RC1/cmake")
endif()

find_package(DynamoRIO REQUIRED)
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

set(JANUS_API_SRCS
	${DYN_SRC_DIR}/core/front_end.c
	${DYN_SRC_DIR}/core/handler.c
    ${DYN_SRC_DIR}/core/code_gen.c
    ${REWRITE_RULE_SOURCES}
    ${REWRITE_RULE_ARCH_SOURCES}
)

set(DYN_INC_PATH
	${DYN_SRC_DIR}/core
	${DYN_SRC_DIR}/include
	${REWRITE_RULE_INCLUDE}
	${REWRITE_RULE_ARCH_INCLUDE}
)

include_directories(${DYN_INC_PATH})

#automatic paralleliser
add_subdirectory(parallel)
#profiler
add_subdirectory(coverage)
#timer
if (JANUS_X86_SUPPORT)
add_subdirectory(prefetcher)
#automatic vector
add_subdirectory(vector)
add_subdirectory(planner)
endif()

