#!/bin/bash

function run_test(){
    echo "TEST NAME: $2" >> test_log.txt #This is used by the python script to regex the test name
    echo -e "\033[1m------ Test: v2/$2 ------\033[0m"
    if [[ $# -ge 1 ]] && [[ $1 = '-n' ]];
    then
        echo "NATIVE TEST NAME: $2" >> native_running_times.txt #This is used by the python script 
        ./$2 >> native_running_times.txt
        echo -n "Running native and jpar_all test -- "
        rm ./$2.* 2> /dev/null
        rm ./$2_map.out 2> /dev/null
        ./../../../janus/lcov ./$2 > /dev/null
        echo -n "lcov done, "
        ./../../../janus/plan ./$2 > /dev/null
        echo  "plan done!"
    fi
    if [[ $# -ge 1 ]] && [[ $1 = '-f' ]] || [[ ! -f $2.jrs ]];
    then
        echo -n "Running jpar_all test -- "
        rm ./$2.* 2> /dev/null
        rm ./$2_map.out 2> /dev/null
        ./../../../janus/lcov ./$2 > /dev/null
        echo -n "lcov done, "
        ./../../../janus/plan ./$2 > /dev/null
        echo  "plan done!"
    fi
    timeout 60 ./../../../janus/jpar -t 4 "./$2" >> test_log.txt
    python3 ./test_parse_log.py #This finds the last finished test written to the log file and prints it
}

#Full output from all tests is stored in test_log.txt
#That file is also used by test_parse_log.py to print short bits of info to STDOUT
rm  ./test_log.txt
run_test "$*" "2mm"
run_test "$*" "2mm_float"
run_test "$*" "3mm"
run_test "$*" "cholesky"
run_test "$*" "cholesky_d"
run_test "$*" "cholesky_float"
run_test "$*" "cholesky_alt"
run_test "$*" "doitgen"
run_test "$*" "gramschmidt"
run_test "$*" "gramschmidt_float"
run_test "$*" "gramschmidt_d"
run_test "$*" "loop_1D2A"
run_test "$*" "loop_1D2A_d"
run_test "$*" "loop_2D2A"
run_test "$*" "loop_2D2A_d"
run_test "$*" "loop_3D2A"
run_test "$*" "lu"
run_test "$*" "lu_float"
run_test "$*" "tislov"
run_test "$*" "tislov_full"
run_test "$*" "nussinov"
run_test "$*" "heat-3d"
run_test "$*" "heat-3d_float"
run_test "$*" "jacobi-2d"
run_test "$*" "fdtd-2d"
#To add a new test:
#1. Add the binary to same folder as this script
#2. Add a run_test call here
#3. Optionally, in test_parse_log.py get_expected_native_time 
#   add the expected native execution time for the test

python3 ./test_parse_log.py -s #Prints a short summary of all tests
