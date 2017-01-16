#!/bin/sh

: ${PSQL=./opt/bin/psql}
: ${HOST=localhost}
: ${DB=postgres}

if [ $# -ne 1 ]
then
    echo "Incorrect number of arguments, expecting only one string containig the SQL query."
    while test $# -ne 0
    do
    	echo "- $1"
    	shift
    done
    exit 1
else
    QUERY=$1
fi

#verbose: true required to get the "Output" field
${PSQL} -h ${HOST} -d ${DB} -c 'explain (format json, verbose true) '"${QUERY};" \
    | sed -e 's, *+$,,' \
    | head -n -2 \
    | tail -n +3 \
    #| ViDa driver
