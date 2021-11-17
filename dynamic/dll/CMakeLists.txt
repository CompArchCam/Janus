set(DLL_SRCS
	dll.cpp
)

set_source_files_properties(${DLL_SRCS} PROPERTIES COMPILE_FLAGS "-std=c++11 -O3")

add_library(jdll SHARED ${DLL_SRCS} ${JANUS_API_SRCS})

#Link to dynamoRIO client
configure_DynamoRIO_client(jdll)
use_DynamoRIO_extension(jdll drcontainers)
use_DynamoRIO_extension(jdll drsyms)
