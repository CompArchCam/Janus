#!/bin/bash

function compile(){
    gcc src/$1.c src/computationKernels.c -o $1 -O1 -lm -g -static -fno-tree-vectorize

}

if [[ $# -ge 1 ]];
then
    compile "$1" #Compile a specific test by name
else
    compile "2mm"
fi
