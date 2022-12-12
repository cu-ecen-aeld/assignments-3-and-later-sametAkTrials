#!/bin/sh

paramNum=$# 
writefile=$1
writestr=$2

if [ 2 -ne $paramNum ]
then
	echo "Missing parameter !"
	exit 1
else
	echo "No Problem about Params"
fi

mkdir -p "$(dirname "$writefile")" && touch "$writefile"

echo "$writestr" > "$writefile"

if [ -d "$(dirname "$writefile")" ]
then
	echo "File exists at the ${writefile}."
else
	echo "File could not be created at ${writefile} !"
	exit 1
fi


