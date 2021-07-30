#!/bin/bash

BUILD_DIRECTORY=$1
RX_DEPTH=$2
REPETITIONS=$3
WARMUP=$4

DEVICE=`cat ${BUILD_DIRECTORY}/configuration/testing.json | jq -r '.["test_executor"]["device"]'`
PORT=`cat ${BUILD_DIRECTORY}/configuration/testing.json | jq -r '.["test_executor"]["port"]'`
RESULT=$(jq -j '.devices[] | select(.name=="'${DEVICE}'") | "\(.ip_address);"' ${BUILD_DIRECTORY}/configuration/devices.json)
IFS=";" read IP <<< $RESULT


cmd="
  ${BUILD_DIRECTORY}/bin/executor_manager\
  -a ${IP}\
  -p ${PORT}\
  -v\
  -f ${BUILD_DIRECTORY}/tests/server.json\
  -x ${RX_DEPTH}\
  -r $REPETITIONS\
  --warmup-iters $WARMUP\
  --max-inline-data 0
"
echo ${cmd}
${cmd} > ${BUILD_DIRECTORY}/tests/test_serverless_server 2>&1 &
pid=$!
echo $pid > ${BUILD_DIRECTORY}/tests/test_server.pid
# wait for the server to boot and start listening for events
sleep 1
