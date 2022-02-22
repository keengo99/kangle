#!/bin/bash
BIN_FILE=$(readlink -f $0)
PROJECT_PATH=$(dirname $BIN_FILE)
echo $PROJECT_PATH
export PATH=$PATH:$GOROOT/bin
export GOPROXY=https://goproxy.io,direct
if [ ! -d $PROJECT_PATH/bin ] ; then
	mkdir $PROJECT_PATH/bin
fi
cd $PROJECT_PATH/test_server
go build -o ../bin/main.exe test_server/main
