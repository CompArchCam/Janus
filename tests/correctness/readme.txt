Test suite developed by Karlis, ks980

The different versions (v0, v1, v2) correspond to the same source code, but compiled with different compiler settings

To run all tests in a version: go to a version folder and run ./run_tests.sh, adding an -f option if lcov and planner needs to be re-run too. This script redirects all Janus output to test_log.txt, and then uses test_parse_log.py to parse the full output and get whether each test was successful or not. 
The tests have built in correctness tests, but test_parse_log.py also verifies whether the correct loops were selected for parallelization
Each test will also give a speedup on the native running time, however, first the native_running_times.txt file must be created.
It can be done by passing the -fn option to ./run_tests.sh, which will also run the binaries natively. 
Afterwards, ./run_tests.sh can be run normally, and the native_running_times.txt file will be reused.

To add a test: currently the source code for all tests is shared in computationKernels.c
Then the MU_test suite asserts and everything is inoved from a file, separate for every test
The root folder has a compile_tests.sh script, which will compile every test, and put it in its directory. Add your separate test file to the script
Optionally pass a parameter to compile_tests.sh it to compile only one specific test




