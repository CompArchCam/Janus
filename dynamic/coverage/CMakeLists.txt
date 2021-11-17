
set(LTIMER_SRCS
	LoopCov.cpp
)

set(FTIMER_SRCS
	FuncCov.cpp
)

set(COUNTER_SRCS
	Counter.cpp
)

set_source_files_properties(${LTIMER_SRCS} PROPERTIES COMPILE_FLAGS "-std=c++11 -O3")
set_source_files_properties(${FTIMER_SRCS} PROPERTIES COMPILE_FLAGS "-std=c++11 -O3")

add_library(jlcov SHARED ${LTIMER_SRCS} ${JANUS_API_SRCS})
add_library(jfcov SHARED ${FTIMER_SRCS} ${JANUS_API_SRCS})
add_library(jcount SHARED ${COUNTER_SRCS})

#Link to dynamoRIO client
configure_DynamoRIO_client(jlcov)
use_DynamoRIO_extension(jlcov drcontainers)
use_DynamoRIO_extension(jlcov drsyms)
configure_DynamoRIO_client(jfcov)
use_DynamoRIO_extension(jfcov drcontainers)
configure_DynamoRIO_client(jcount)
use_DynamoRIO_extension(jfcov drcontainers)
