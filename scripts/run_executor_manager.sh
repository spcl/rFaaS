#!/bin/bash

BUILD_DIRECTORY=$1
DEVICE_DATABASE=$2

DEVICE=$(cat ${BUILD_DIRECTORY}/tests/configuration/testing.json | jq -r '.["executor_manager_server"]["device"]')
PORT=$(cat ${BUILD_DIRECTORY}/tests/configuration/testing.json | jq -r '.["executor_manager_server"]["port"]')
RESULT=$(jq -j '.devices[] | select(.name=="'${DEVICE}'") | "\(.ip_address);"' ${BUILD_DIRECTORY}/tests/configuration/devices.json)
RESULT_STATUS=$?
if [ ${RESULT_STATUS} -ne 0 ]; then
	echo "Incorrect parsing of ${BUILD_DIRECTORY}/tests/configuration/devices.json!"
	echo "Result: ${RESULT}"
	exit 1
fi

# Generate configuration
mgr_cfg='
{
  "config": {
    "rdma_device": "",
    "rdma_device_port": 0,
    "resource_manager_address": "",
    "resource_manager_port": 10000,
    "resource_manager_secret": 42
  },
  "executor": {
    "use_docker": false,
    "repetitions": 1000,
    "warmup_iters": 0,
    "pin_threads": false
  }
}
'
jq --arg device $DEVICE --argjson port $PORT '.config.rdma_device = $device | .config.rdma_device_port = $port' <<<${mgr_cfg} >${BUILD_DIRECTORY}/tests/executor_manager.json

IFS=";" read IP <<<$RESULT
jq --arg addr $IP --argjson port $PORT --argjson cores 1 '.executors += [{"address": $addr, "port": $port, "cores": $cores}]' \
	<<<"{}" \
	>${BUILD_DIRECTORY}/tests/configuration/executor_database.json

# Generate database

cmd="
  ${BUILD_DIRECTORY}/bin/executor_manager\
  --config ${BUILD_DIRECTORY}/tests/executor_manager.json\
  --device-database ${DEVICE_DATABASE}\
  --skip-resource-manager\
  -v
"
${cmd} >${BUILD_DIRECTORY}/tests/test_serverless_server 2>&1 &
pid=$!
echo $pid $ret >${BUILD_DIRECTORY}/tests/test_server.pid
# wait for the server to boot and start listening for events
sleep 1
# exit with the return code of executor_manager
[ -d "/proc/${pid}" ] && exit 0 || exit 1
