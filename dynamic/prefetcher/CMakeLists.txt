set(OPT_C_SRCS
	#prefetch.cpp
	prefetch.c
	pfhandler.c
)

set_source_files_properties(${OPT_C_SRCS} PROPERTIES COMPILE_FLAGS " -O3 -Wall")

#add_library(gpft SHARED ${OPT_C_SRCS} ${JANUS_API_SRCS})
add_library(jfetch SHARED ${OPT_C_SRCS} ${JANUS_API_SRCS})

configure_DynamoRIO_client(jfetch)
use_DynamoRIO_extension(jfetch drcontainers)
