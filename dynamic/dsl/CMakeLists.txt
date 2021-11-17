set(DSL_SRCS
	dsl_core.cpp
	dsl_handler.cpp
	func.cpp
        util.cpp
)

set_source_files_properties(${DSL_SRCS} PROPERTIES COMPILE_FLAGS "-std=c++11 -O3")

#add_library(janus SHARED ${DSL_SRCS} ${JANUS_API_SRCS})
add_library(jdsl SHARED ${DSL_SRCS} ${JANUS_API_SRCS})

#Link to dynamoRIO client
configure_DynamoRIO_client(jdsl)
use_DynamoRIO_extension(jdsl drcontainers)
