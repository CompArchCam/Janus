
set(PLAN_C_SRCS
	plan.c
)

set(PLAN_CXX_SRCS
	plan.cpp
)

set_source_files_properties(${PLAN_CXX_SRCS} PROPERTIES COMPILE_FLAGS "-std=c++11 -O3")

add_library(jplan SHARED ${PLAN_C_SRCS} ${PLAN_CXX_SRCS} ${JANUS_API_SRCS})

#Link to dynamoRIO client
configure_DynamoRIO_client(jplan)
use_DynamoRIO_extension(jplan drcontainers)
use_DynamoRIO_extension(jplan drsyms)
