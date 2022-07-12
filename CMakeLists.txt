cmake_minimum_required (VERSION 2.6)

project(janus)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")

set (CMAKE_CXX_STANDARD 20)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	message("Detected x86-64 system")
	option(JANUS_X86_SUPPORT "x86 support" ON)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
	message("Detected AArch64 system")
	option(JANUS_ARM64_SUPPORT "AArch64 support" ON)
endif ()

#define source for janus rewrite schedule interface
set(REWRITE_RULE_SOURCES
	${PROJECT_SOURCE_DIR}/shared/rule_isa.c
	${PROJECT_SOURCE_DIR}/shared/jvar/janus_var.c
)

set(REWRITE_RULE_SOURCES_CPP
	${PROJECT_SOURCE_DIR}/shared/rrules/vector/VECT_rule_structs.cpp
)

if (JANUS_X86_SUPPORT)
	add_definitions(-DJANUS_X86)
	set(REWRITE_RULE_ARCH_SOURCES
		${PROJECT_SOURCE_DIR}/shared/arch/x86/janus_x86.c
	)
	set(REWRITE_RULE_ARCH_INCLUDE
		${PROJECT_SOURCE_DIR}/shared/arch/x86/
	)
elseif (JANUS_ARM64_SUPPORT)
	add_definitions(-DJANUS_AARCH64)
	set(REWRITE_RULE_ARCH_SOURCES
		${PROJECT_SOURCE_DIR}/shared/arch/aarch64/janus_aarch64.c
	)
	set(REWRITE_RULE_ARCH_INCLUDE
		${PROJECT_SOURCE_DIR}/shared/arch/aarch64/
	)
endif ()

set(REWRITE_RULE_INCLUDE
	${PROJECT_SOURCE_DIR}/shared/
	${PROJECT_SOURCE_DIR}/shared/arch/
	${PROJECT_SOURCE_DIR}/shared/format/
	${PROJECT_SOURCE_DIR}/shared/rrules/vector/
	${PROJECT_SOURCE_DIR}/shared/jvar/
	${PROJECT_SOURCE_DIR}/shared/profile/
	${PROJECT_SOURCE_DIR}/shared/rrules/feedback/
	${PROJECT_SOURCE_DIR}/shared/rrules/opt/
)

#enable testing for parallelisation and prefetch
enable_testing()

if(VERBOSE)
	add_definitions(-DJANUS_VERBOSE)
endif(VERBOSE)

include_directories(shared)

add_subdirectory(static)
add_subdirectory(dynamic)
#add tests for parallelisation and prefetch
add_subdirectory(tests)
