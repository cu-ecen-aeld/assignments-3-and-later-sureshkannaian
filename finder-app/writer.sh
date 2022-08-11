#!/bin/bash
# Author: Suresh Kannaian

writefile=$1
writestr=$2

if [ $# -lt 2 ]
then
    echo "Missing argument writefile"
	echo "Missing argument writestr"
    exit 1	
fi

echo $writestr >> $writefile