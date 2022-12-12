#!/bin/sh

paramNum=$# 
filesdir=$1
searchstr=$2

if [ 2 -ne $paramNum ]
then
	echo "Missing parameter !"
	exit 1
# else
	# echo "No Problem about Params"
fi

if [ ! -d "$filesdir" ] 
then
	echo "File doesn't exist at ${filesdir} !"
	exit 1
else
	echo "File exists at ${filesdir}."
fi

foundedLineCount=$(grep -r $searchstr $filesdir* | wc -l)
foundedFileCount=$(grep -rl $searchstr $filesdir* | wc -l)


echo "The number of files are $foundedFileCount and the number of matching lines are $foundedLineCount"

