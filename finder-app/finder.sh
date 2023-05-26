#!/usr/bin/bash

# validate inputs
if [ $# -ne 2 ]; then
    echo "Incorrect number of arguments"
    exit 1
fi

if [ -z "$1" ]; then
    echo "null argument 1, expected filesdir"
    exit 1
else
    filesdir="$1"
fi

if [ ! -d "$filesdir" ]; then
    echo "$filesdir is not a directory"
    exit 1
fi

if [ -z "$2" ]; then
    echo "null argument 2, expected searchstr"
    exit 1
else
    searchstr="$2"
fi

# count files
numfiles=`find $filesdir -type f|wc -l`

# count matches
matchlines=`grep -r "$searchstr" $filesdir|wc -l`

echo "The number of files are $numfiles and the number of matching lines are $matchlines"
