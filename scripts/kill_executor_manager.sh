#!/bin/bash

BUILD_DIRECTORY=$1

pid=`cat ${BUILD_DIRECTORY}/tests/test_server.pid`
kill ${pid}
