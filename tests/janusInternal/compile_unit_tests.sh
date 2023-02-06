#!/bin/bash

function compile(){
    gcc unitTests/$1.c -o $1 -O1 -lm -g -static -fno-tree-vectorize

}

if [[ $# -ge 1 ]];
then
    compile "$1" #Compile a specific test by name
else
    compile "OpenAndParseBinary"
fi
