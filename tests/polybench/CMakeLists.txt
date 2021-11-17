if (JANUS_X86_SUPPORT)
	set(POLY_TEST_DIRECTORY "${PROJECT_SOURCE_DIR}/tests/polybench/x86-gcc/")
elseif (JANUS_ARM64_SUPPORT)
	set(POLY_TEST_DIRECTORY "${PROJECT_SOURCE_DIR}/tests/polybench/arm-gcc/")
endif ()

message(STATUS "Generating Polybench tests")

add_test(NAME 2mm.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./2mm)

add_test(NAME 2mm.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 2mm)

add_test(NAME 3mm.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./3mm)

add_test(NAME 3mm.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 3mm)

add_test(NAME adi.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./adi)

add_test(NAME adi.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 adi)

add_test(NAME atax.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./atax)

add_test(NAME atax.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 atax)

add_test(NAME bicg.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./bicg)

add_test(NAME bicg.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 bicg)

add_test(NAME cholesky.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./cholesky)

add_test(NAME cholesky.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 cholesky)

add_test(NAME correlation.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./correlation)

add_test(NAME correlation.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 correlation)

add_test(NAME covariance.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./covariance)

add_test(NAME covariance.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 covariance)

add_test(NAME deriche.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./deriche)

add_test(NAME deriche.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 deriche)

add_test(NAME doitgen.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./doitgen)

add_test(NAME doitgen.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 doitgen)

add_test(NAME fdtd-2d.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./fdtd-2d)

add_test(NAME fdtd-2d.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 fdtd-2d)

add_test(NAME floyd-warshall.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./floyd-warshall)

add_test(NAME floyd-warshall.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 floyd-warshall)

add_test(NAME gemm.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./gemm)

add_test(NAME gemm.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 gemm)

add_test(NAME gemver.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./gemver)

add_test(NAME gemver.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 gemver)

add_test(NAME gesummv.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./gesummv)

add_test(NAME gesummv.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 gesummv)

add_test(NAME gramschmidt.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./gramschmidt)

add_test(NAME gramschmidt.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 gramschmidt)

add_test(NAME heat-3d.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./heat-3d)

add_test(NAME heat-3d.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 heat-3d)

add_test(NAME jacobi-1d.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./jacobi-1d)

add_test(NAME jacobi-1d.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 jacobi-1d)

add_test(NAME jacobi-2d.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./jacobi-2d)

add_test(NAME jacobi-2d.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 jacobi-2d)

add_test(NAME lu.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./lu)

add_test(NAME lu.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 lu)

add_test(NAME ludcmp.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./ludcmp)

add_test(NAME ludcmp.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 ludcmp)

add_test(NAME mvt.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./mvt)

add_test(NAME mvt.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 mvt)

add_test(NAME nussinov.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./nussinov)

add_test(NAME nussinov.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 nussinov)

add_test(NAME seidel-2d.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./seidel-2d)

add_test(NAME seidel-2d.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 seidel-2d)

add_test(NAME symm.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./symm)

add_test(NAME symm.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 symm)

add_test(NAME syr2k.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./syr2k)

add_test(NAME syr2k.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 syr2k)

add_test(NAME syrk.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./syrk)

add_test(NAME syrk.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 syrk)

add_test(NAME trisolv.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./trisolv)

add_test(NAME trisolv.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 trisolv)

add_test(NAME trmm.native
		 WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		 COMMAND ./trmm)

add_test(NAME trmm.parallel
		WORKING_DIRECTORY ${POLY_TEST_DIRECTORY}
		COMMAND ../../../janus/jpar 4 trmm)
