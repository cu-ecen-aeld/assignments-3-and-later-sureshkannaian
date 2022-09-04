#!/bin/sh
# Author: Suresh Kannaian

filesdir=$1
searchstr=$2

if [ $# -lt 2 ]
then
    echo "Missing argument filesdir"
	echo "Missing argument search string"
    exit 1	
else

	if [ ! -d "$filesdir" ]
	then
        echo "$filesdir doesn't exist"
		exit 1
	fi	
fi

lines=0
numfiles=0


while read -r line ; do
    lines=$(($lines + $line))
    let numfiles=numfiles+1
done <<<$(grep -h -c -r "$searchstr" $filesdir)


echo "The number of files are $numfiles and the number of matching lines are $lines"
