#!/bin/bash

CC='gcc'

if [ $(uname -m) == 'aarch64' ]; then
  OUT='../aarch64'
elif [ $(uname -m) == 'x86_64' ]; then
  OUT='../x86'
fi

#test 1
echo "test 1: const bound"
echo "$CC -O2 doall_const_bound.c -o $OUT/doall_const_bound"
$CC -O2 doall_const_bound.c -o $OUT/doall_const_bound

#test 2
echo "test 2: register bound"
echo "$CC -O2 doall_var_bound.c -o $OUT/doall_var_bound"
$CC -O2 doall_var_bound.c -o $OUT/doall_var_bound

#test 3
echo "test 3: stack bound"
echo "$CC -O1 doall_const_bound.c -o $OUT/doall_stack_bound"
$CC doall_const_bound.c -o $OUT/doall_stack_bound

#test 4
echo "test 4: static"
echo "$CC -O2 static.c -o $OUT/static"
$CC -O2 static.c -o $OUT/static
