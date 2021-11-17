#!/bin/bash

# https://stackoverflow.com/questions/34245322/bash-using-stdin-in-if-statement
# https://stackoverflow.com/questions/2456750/how-to-check-if-stdin-is-from-the-terminal-or-a-pipe-in-a-shell-script
stdin=""
if [ ! -t 0 ]
then stdin=$(readlink -n /proc/self/fd/0)
fi
echo $stdin
echo $@
num=10
for i in $(seq $num)
do
    if [ -t 0 ]
    then runtimes[$i]=$(/usr/bin/time -f '%e' $@ 2>&1 | tail -n 1)
    else runtimes[$i]=$(/usr/bin/time -f '%e' $@ <$stdin | tail -n 1)
    fi
    echo $i "${runtimes[$i]}"
done

# https://stackoverflow.com/questions/7442417/how-to-sort-an-array-in-bash
IFS=$'\n' sorted=($(sort -g <<<"${runtimes[*]}"))
unset IFS

top=$(($num-1))
min=${sorted[0]}
max=${sorted[$top]}
med=0

if [ $(($num%2)) -eq 0 ]
then
    highmid=$(($num/2))
    lowmid=$(($highmid-1))
    med=$(bc <<< "scale=2; (${sorted[$lowmid]} + ${sorted[$highmid]})/2")
else
    mid=$((($num-1)/2))
    med=${sorted[$mid]}
fi

echo "$med Median"
echo "$max Maximum"
echo "$min Minimum"
