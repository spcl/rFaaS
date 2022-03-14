#!/bin/bash

BUILD_DIRECTORY=$1
RX_DEPTH=$2
REPETITIONS=$3
WARMUP=$4

DEVICE=`cat ${BUILD_DIRECTORY}/configuration/testing.json | jq -r '.["rfaas_server"]["device"]'`
PORT=`cat ${BUILD_DIRECTORY}/configuration/testing.json | jq -r '.["rfaas_server"]["port"]'`
RESULT=$(jq -j '.devices[] | select(.name=="'${DEVICE}'") | "\(.ip_address);"' ${BUILD_DIRECTORY}/configuration/devices.json)
RESULT_STATUS=$?
if [ ${RESULT_STATUS} -ne 0 ]; then
  echo "Incorrect parsing of ${BUILD_DIRECTORY}/configuration/devices.json!"
  echo "Result: ${RESULT}"
  exit 1
fi

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
echo $pid $ret > ${BUILD_DIRECTORY}/tests/test_server.pid
# wait for the server to boot and start listening for events
sleep 1
# exit with the return code of executor_manager
[ -d "/proc/${pid}" ] && exit 0 || exit 1
