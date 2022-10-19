#! /usr/bin/bash

NC='\033[0m'
RED='\033[31m'
GREEN='\033[32m'

function init() {
    for file in *.loop.select;
    do 
        filename="${file%.loop.select}"
        dirname="$filename"_test
        echo $file
        echo $filename
        echo $dirname
        mkdir $dirname
        cp $file $dirname
        cp $filename $dirname
    done
}

function parallelisation_test() {
    local aggregate=""
    for dir in *_test;
    do
        name="${dir%_test}"
        echo ">>>>>>>>>>$name<<<<<<<<<<"
        cd $dir
        ../../../../bin/analyze -p $name > "$name".seleted_final 2>memoryleak.out
        ../../../../bin/schedump "$name".jrs > "$name".jrs.dump
        diff "$name".jrs.dump "$name".jrs.master.dump
        if [ $? -ne 0 ];
        then
            aggregate="$aggregate"" ""$name"
            echo -e "$RED ERROR $NC"
        fi
        cd ..
    done

    if [[ $aggregate != "" ]]; then
        echo -e "$RED $aggregate $NC has failed, check specific test outputs."
    else
        echo -e "$GREEN PASS $NC"
    fi
}

function cfg_test() {
    local loop_aggregate=""
    local proc_aggregate=""
    for dir in *_test;
    do
        name="${dir%_test}"
        echo ">>>>>>>>>>$name<<<<<<<<<<"
        cd $dir
        ../../../../bin/analyze -cfg $name > "$name".cfg_out
        diff "$name".loop.cfg "$name".master.loop.cfg
        if [ $? -ne 0 ];
        then
            loop_aggregate="$loop_aggregate"" ""$name"
            echo -e "$RED ERROR: loop cfg differs $NC"
        else
            echo -e "$GREEN PASS: loop cfg $NC"
        fi

        diff "$name".proc.cfg "$name".master.proc.cfg
        if [ $? -ne 0 ];
        then
            proc_aggregate="$proc_aggregate"" ""$name"
            echo -e "$RED ERROR: proc cfg differs $NC"
        else
            echo -e "$GREEN PASS: proc cfg $NC"
        fi
        cd ..
    done
    if [[ $loop_aggregate != "" ]]; then
        echo -e "$RED FAILED: loop cfg differs in: $loop_aggregate $NC"
    else
        echo -e "$GREEN PASS: all loop cfg passed regression test $NC"
    fi
    if [[ $proc_aggregate != "" ]]; then
        echo -e "$RED FAILED: loop cfg differs in: $proc_aggregate $NC"
    else
        echo -e "$GREEN PASS: all loop cfg passed regression test $NC"
    fi
}

case $1'' in
    'init')
        init;;
    'par_test')
        parallelisation_test;;
    'cfg_test')
        cfg_test;;
esac
