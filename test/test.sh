#!/bin/bash
if [ ! -d ext ] ; then
	mkdir ext
fi
cp ../webadmin . -a
if [ ! -f ./bin/main.exe ] ; then
	echo "please call ./build.sh first"
	exit 1
fi
if [ -f ../Debug/kangle.exe ] ; then
	./bin/main.exe -e ../Debug
	exit 1
fi
./bin/main.exe -e ../build
