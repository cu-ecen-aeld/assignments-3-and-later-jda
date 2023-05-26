#!/usr/bin/bash

# validate inputs
if [ $# -ne 2 ]; then
    echo "Incorrect number of arguments"
    exit 1
fi

if [ -z "$1" ]; then
    echo "null argument 1, expected writefile"
    exit 1
else
    writefile="$1"
fi

if [ -z "$2" ]; then
    echo "null argument 2, expected writestr"
    exit 1
else
    writestr="$2"
fi

dir=`dirname $writefile`
mkdir -p "$dir"
echo $writestr > $writefile
