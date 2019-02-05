
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_C_FLAGS_DEBUG "-g -DJANUS_VERBOSE")

if (JANUS_X86_SUPPORT)
	set(PARA_ARCH_SRCS
		arch/x86/emit.c
		arch/x86/iterator.c
	)
    set(PARA_INCLUDE_DIR
            include/
            arch/
            arch/x86
            ${PROJECT_SOURCE_DIR}/dynamic/vector
    )
endif (JANUS_X86_SUPPORT)

if (JANUS_ARM64_SUPPORT)
    set(PARA_ARCH_SRCS
            arch/aarch64/emit.c
            arch/aarch64/iterator.c
    )
    set(PARA_INCLUDE_DIR
            include/
            arch/
            arch/aarch64
    )
endif (JANUS_ARM64_SUPPORT)

set(PARA_SRCS
	core.c
	control.c
	jthread.c
	loop.c
	stats.c
	rcheck.c
	#stm.c
	#stm_internal.c
	#sync.c
	#hashtable.c	
)

if (JANUS_X86_SUPPORT)
set(VECTOR_SRCS
	${PROJECT_SOURCE_DIR}/dynamic/vector/vhandler.cpp
	${PROJECT_SOURCE_DIR}/dynamic/vector/extend.cpp
	${PROJECT_SOURCE_DIR}/dynamic/vector/init.cpp
	${PROJECT_SOURCE_DIR}/dynamic/vector/reduce.cpp
	${PROJECT_SOURCE_DIR}/dynamic/vector/broadcast.cpp
	${PROJECT_SOURCE_DIR}/dynamic/vector/utilities.cpp
)
endif (JANUS_X86_SUPPORT)

if (JANUS_ARM64_SUPPORT)
set(VECTOR_SRCS)
endif (JANUS_ARM64_SUPPORT)

include_directories(${PARA_INCLUDE_DIR})
#set_source_files_properties(${PARA_C_SRCS} PROPERTIES COMPILE_FLAGS "-O3 -g")

add_library(jpar SHARED ${PARA_SRCS}
						${PARA_ARCH_SRCS}
						${VECTOR_SRCS}
						${REWRITE_RULE_SOURCES_CPP}
						${JANUS_API_SRCS})

#Link to dynamoRIO client
configure_DynamoRIO_client(jpar)
use_DynamoRIO_extension(jpar drcontainers)
