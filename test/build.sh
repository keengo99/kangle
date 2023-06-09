#!/bin/sh
BIN_FILE=$(readlink -f $0)
PROJECT_PATH=$(dirname $BIN_FILE)
export PATH=$PATH:$GOROOT/bin
export GOPROXY=https://goproxy.cn,direct
export GO111MODULE=on
if [ ! -d $PROJECT_PATH/bin ] ; then
	mkdir $PROJECT_PATH/bin
fi
cd $PROJECT_PATH/test_server
echo "building main ..."
go mod tidy
go build -o ../bin/main.exe test_server/main
go build -o ../bin/benchmark.exe test_server/benchmark
