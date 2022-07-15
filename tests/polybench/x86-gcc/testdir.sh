#! /usr/bin/bash
for file in *.loop.select;
do 
    filename="${file%.loop.select}"
    dirname="$filename"_test
    echo $file
    echo $filename
    echo $dirname
    mkdir $dirname
    cp "$file" "$direname"
    cp $filename $dirname
done
