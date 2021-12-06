#!/bin/bash

function compile(){
    gcc src/$1.c src/computationKernels.c -o $1 -O1 -lm -g -static -fno-tree-vectorize

}

if [[ $# -ge 1 ]];
then
    compile "$1" #Compile a specific test by name
else
    compile "2mm"
    compile "2mm_float"
    compile "3mm"
    compile "cholesky"
    compile "cholesky_d"
    compile "cholesky_float"
    compile "cholesky_alt"
    compile "doitgen"
    compile "gramschmidt"
    compile "gramschmidt_float"
    compile "gramschmidt_d"
    compile "loop_1D2A"
    compile "loop_1D2A_d"
    compile "loop_2D2A"
    compile "loop_2D2A_d"
    compile "loop_3D2A"
    compile "lu"
    compile "lu_float"
    compile "tislov"
    compile "tislov_full"
    compile "nussinov"
    compile "heat-3d"
    compile "heat-3d_float"
    compile "jacobi-2d"
    compile "fdtd-2d"
fi
