set(OPT_C_SRCS
	prefetch.cpp
)

set_source_files_properties(${OPT_C_SRCS} PROPERTIES COMPILE_FLAGS " -O3 -Wall")

add_library(gpft SHARED ${OPT_C_SRCS} ${JANUS_API_SRCS})

configure_DynamoRIO_client(gpft)
use_DynamoRIO_extension(gpft drcontainers)
