set(VECTOR_SRCS
	vector.cpp
	vhandler.cpp
	extend.cpp
	init.cpp
	reduce.cpp
	broadcast.cpp
	utilities.cpp
)

add_library(jvect SHARED ${VECTOR_SRCS} ${REWRITE_RULE_SOURCES_CPP} ${JANUS_API_SRCS})

#Link to dynamoRIO client
configure_DynamoRIO_client(jvect)
use_DynamoRIO_extension(jvect drcontainers)
