if (JANUS_X86_SUPPORT)
	set(POLY_TEST_DIRECTORY "${PROJECT_SOURCE_DIR}/tests/units/x86/")
elseif (JANUS_ARM64_SUPPORT)
	set(POLY_TEST_DIRECTORY "${PROJECT_SOURCE_DIR}/tests/units/aarch64/")
endif ()

message(STATUS "Generating Polybench tests")

add_test(NAME doall_const_bound.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./doall_const_bound)

add_test(NAME doall_const_bound.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 doall_const_bound)

add_test(NAME doall_stack_bound.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./doall_stack_bound)

add_test(NAME doall_stack_bound.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 doall_stack_bound)

add_test(NAME doall_var_bound.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./doall_var_bound)

add_test(NAME doall_var_bound.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 doall_var_bound)