#!/bin/sh
BIN_FILE=$(readlink -f $0)
PROJECT_PATH=$(dirname $BIN_FILE)
cp -a $PROJECT_PATH/../webadmin $PROJECT_PATH
if [ ! -f $PROJECT_PATH/bin/main.exe ] ; then
	echo "please call $PROJECT_PATH/build.sh first"
	exit 1
fi
$PROJECT_PATH/bin/main.exe -e $PROJECT_PATH/../build -m=true
