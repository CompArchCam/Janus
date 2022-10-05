#! /usr/bin/bash
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

function run_test() {
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
            echo "ERROR"
        fi
        cd ..
    done
    echo "$aggregate has failed"
}

case $1'' in
    'init')
        init;;
    'test')
        run_test;;
esac
