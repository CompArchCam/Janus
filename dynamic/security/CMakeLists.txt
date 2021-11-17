
set(GUARD_SRCS
	JanusGuard.cpp
)

set_source_files_properties(${GUARD_SRCS} PROPERTIES COMPILE_FLAGS "-std=c++11 -O3")

add_library(jguard SHARED ${GUARD_SRCS} ${JANUS_API_SRCS})

#Link to dynamoRIO client
configure_DynamoRIO_client(jguard)
use_DynamoRIO_extension(jguard drcontainers)
use_DynamoRIO_extension(jguard drsyms)
