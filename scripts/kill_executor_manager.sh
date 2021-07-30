#!/bin/bash

BUILD_DIRECTORY=/home/mcopik/Projekty/ETH/serverless/2021_rfaas/repo/build

pid=`cat ${BUILD_DIRECTORY}/tests/test_server.pid`
kill ${pid}
