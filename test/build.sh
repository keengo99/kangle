#!/bin/bash
BIN_FILE=$(readlink -f $0)
PROJECT_PATH=$(dirname $BIN_FILE)
echo $PROJECT_PATH
export PATH=$PATH:$GOROOT/bin
export GOPROXY=https://goproxy.io,direct
cd $PROJECT_PATH/test_server
go build -o ../main.exe test_server/main
