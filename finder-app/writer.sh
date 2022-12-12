#!/bin/sh

paramNum=$# 
writefile=$1
writestr=$2

if [ 2 -ne $paramNum ]
then
	echo "Missing parameter !"
	return 1
fi

mkdir -p "$(dirname "$writefile")" && touch "$writefile"

echo "$writestr" > "$writefile"

if [ -d "$(dirname "$writefile")" ]
then
	echo "File exists at the ${writefile}."
else
	echo "File could not be created at ${writefile} !"
	return 1
fi


