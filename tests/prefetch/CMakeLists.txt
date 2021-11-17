#test NAS.integer_sort
if(PERF_TEST)
	add_test(NAME nas.integer_sort.native
			 WORKING_DIRECTORY ${TEST_DIRECTORY}/nas/is
			 COMMAND ./is-no)
endif(PERF_TEST)

add_test(NAME nas.integer_sort.prefetch
		 WORKING_DIRECTORY ${TEST_DIRECTORY}/nas/is
		 COMMAND ${PROJECT_SOURCE_DIR}/janus/jpft is-no)

if(PERF_TEST)
	add_test(NAME nas.hash_join2.native
			 WORKING_DIRECTORY ${TEST_DIRECTORY}/nas/hj2
			 COMMAND ./hj2-no -a NPO_st -r 12800000 -s 12800000)
endif(PERF_TEST)

add_test(NAME nas.hash_join2.prefetch
		 WORKING_DIRECTORY ${TEST_DIRECTORY}/nas/hj2
		 COMMAND ${PROJECT_SOURCE_DIR}/janus/jpft hj2-no -a NPO_st -r 12800000 -s 12800000)

#test NAS.hash_join8
if(PERF_TEST)
add_test(NAME nas.hash_join8.native
		 WORKING_DIRECTORY ${TEST_DIRECTORY}/nas/hj8
		 COMMAND ./hj8-no -a NPO_st -r 12800000 -s 12800000)
endif(PERF_TEST)
add_test(NAME nas.hash_join8.prefetch
		 WORKING_DIRECTORY ${TEST_DIRECTORY}/nas/hj8
		 COMMAND ${PROJECT_SOURCE_DIR}/janus/jpft hj8-no -a NPO_st -r 12800000 -s 12800000)

#test NAS.cg
if(PERF_TEST)
add_test(NAME nas.cg.native
		 WORKING_DIRECTORY ${TEST_DIRECTORY}/nas/cg
		 COMMAND ./cg-no)
endif(PERF_TEST)
add_test(NAME nas.cg.prefetch
		 WORKING_DIRECTORY ${TEST_DIRECTORY}/nas/cg
		 COMMAND ${PROJECT_SOURCE_DIR}/janus/jpft cg-no)
