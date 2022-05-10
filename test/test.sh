#!/bin/bash
if [ ! -d ext ] ; then
	mkdir ext
fi
cp ../webadmin . -a
if [ ! -f ./bin/main.exe ] ; then
	echo "please call ./build.sh first"
	exit 1
fi
./bin/main.exe -e ../build -m 1
