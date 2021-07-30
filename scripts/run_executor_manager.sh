
BUILD_DIRECTORY=/home/mcopik/Projekty/ETH/serverless/2021_rfaas/repo/build
IP=192.168.0.18
PORT=10001

RX_DEPTH=$1
REPETITIONS=$2
WARMUP=$3

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
${cmd} > ${BUILD_DIRECTORY}/tests/test_serverless_server 2>&1 &
pid=$!
echo $pid > ${BUILD_DIRECTORY}/tests/test_server.pid
# wait for the server to boot and start listening for events
sleep 1
