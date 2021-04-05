#!/bin/bash

trap 'echo -ne "Stop tests...\n"  && killAllProcesses && exit 1' INT

#define(){ IFS='\n' read -r -d '' ${1} || true; }
#SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
#SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd)"
SCRIPTPATH=$(pwd)

killAllProcesses(){
    if [ ! -z ${rpid+x} ]; then
	    #echo "Killing remote processes with PID$rpid"
	    #ssh ${USER}@${REMOTE_IP} "kill -9 $rpid"
	    ssh ${USER}@${REMOTE_IP} "killall -s 9 executor"
    fi
}

IP=$1
REMOTE_IP=$2
#REMOTE_SERVER=$2
REPETITIONS=$3

CODE_SIZE=12000
LOCAL_PORT=10000
REMOTE_PORT=10001
RX_DEPTH=32
WARMUP=100
MAX_INLINE=128
TIMEOUT=-1

REMOTE_OPTIONS="--polling-mgr thread --max-inline-data ${MAX_INLINE} --pin-threads 1 --func-size ${CODE_SIZE} --timeout ${TIMEOUT}"
LOCAL_OPTIONS="--name empty --max-inline-data ${MAX_INLINE} --functions ${SCRIPTPATH}/examples/libfunctions.so"

killall warm_benchmarker

echo "Run 1 thread, size = 1, reps = $REPETITIONS"
SIZE=1
THREADS=1
${SCRIPTPATH}/tests/warm_benchmarker -a $IP -p ${LOCAL_PORT} -r $REPETITIONS --warmup-iters $WARMUP -s $SIZE -x ${RX_DEPTH} --out-file ${SCRIPTPATH}/test_serverless_result_$SIZE.csv ${LOCAL_OPTIONS} -c $THREADS > ${SCRIPTPATH}/test_serverless_client_${SIZE}_${THREADS} 2>&1 &
client_pid=$!

cmd="taskset 0x3 ${SCRIPTPATH}/bin/executor -a ${IP} -p ${LOCAL_PORT} -v -f ${SCRIPTPATH}/server.json --fast $THREADS -x ${RX_DEPTH} -r $REPETITIONS --warmup-iters $WARMUP -s $SIZE ${REMOTE_OPTIONS}"
sshcmd=( "ssh" -M "${USER}@${REMOTE_IP}" "${cmd} > ${SCRIPTPATH}/test_serverless_server_${SIZE}_${THREADS} 2>&1")
$("${sshcmd[@]}")   

wait ${client_pid}

echo "Server"
tail ${SCRIPTPATH}/test_serverless_server_${SIZE}_${THREADS}
echo "Client"
tail ${SCRIPTPATH}/test_serverless_client_${SIZE}_${THREADS}

echo "Run 1 thread, size = 1024"
SIZE=1024
THREADS=1
${SCRIPTPATH}/tests/warm_benchmarker -a $IP -p ${LOCAL_PORT} -r $REPETITIONS --warmup-iters $WARMUP -s $SIZE -x ${RX_DEPTH} --out-file ${SCRIPTPATH}/test_serverless_result_$SIZE.csv ${LOCAL_OPTIONS} -c $THREADS > ${SCRIPTPATH}/test_serverless_client_${SIZE}_${THREADS} 2>&1 &
client_pid=$!

cmd="taskset 0x3 ${SCRIPTPATH}/bin/executor -a ${IP} -p ${LOCAL_PORT} -v -f ${SCRIPTPATH}/server.json --fast $THREADS -x ${RX_DEPTH} -r $REPETITIONS --warmup-iters $WARMUP -s $SIZE ${REMOTE_OPTIONS}"
sshcmd=( "ssh" -M "${USER}@${REMOTE_IP}" "${cmd} > ${SCRIPTPATH}/test_serverless_server_${SIZE}_$THREADS 2>&1")
$("${sshcmd[@]}")   

wait ${client_pid}

echo "Server"
tail ${SCRIPTPATH}/test_serverless_server_${SIZE}_${THREADS}
echo "Client"
tail ${SCRIPTPATH}/test_serverless_client_${SIZE}_${THREADS}

echo "Run 4 threads, size = 1"
SIZE=1
THREADS=4
${SCRIPTPATH}/tests/parallel_invocations -a $IP -p ${LOCAL_PORT} -r $REPETITIONS --warmup-iters $WARMUP -s $SIZE -x ${RX_DEPTH} --out-file ${SCRIPTPATH}/test_serverless_result_$SIZE.csv ${LOCAL_OPTIONS} -c ${THREADS} > ${SCRIPTPATH}/test_serverless_client_${SIZE}_${THREADS} 2>&1 &
client_pid=$!

cmd="taskset 0x3 ${SCRIPTPATH}/bin/executor -a ${IP} -p ${LOCAL_PORT} -v -f ${SCRIPTPATH}/server.json --fast ${THREADS} -x ${RX_DEPTH} -r $REPETITIONS --warmup-iters $WARMUP -s $SIZE ${REMOTE_OPTIONS}"
sshcmd=( "ssh" -M "${USER}@${REMOTE_IP}" "${cmd} > ${SCRIPTPATH}/test_serverless_server_${SIZE}_$THREADS 2>&1")
$("${sshcmd[@]}")   

wait ${client_pid}

echo "Server"
tail ${SCRIPTPATH}/test_serverless_server_${SIZE}_${THREADS}
echo "Client"
tail ${SCRIPTPATH}/test_serverless_client_${SIZE}_${THREADS}

echo "Run 4 thread, size = 1024"
SIZE=1024
THREADS=4
${SCRIPTPATH}/tests/parallel_invocations -a $IP -p ${LOCAL_PORT} -r $REPETITIONS --warmup-iters $WARMUP -s $SIZE -x ${RX_DEPTH} --out-file ${SCRIPTPATH}/test_serverless_result_$SIZE.csv ${LOCAL_OPTIONS} -c ${THREADS} > ${SCRIPTPATH}/test_serverless_client_${SIZE}_${THREADS} 2>&1 &
client_pid=$!

cmd="taskset 0x3 ${SCRIPTPATH}/bin/executor -a ${IP} -p ${LOCAL_PORT} -v -f ${SCRIPTPATH}/server.json --fast ${THREADS} -x ${RX_DEPTH} -r $REPETITIONS --warmup-iters $WARMUP -s $SIZE ${REMOTE_OPTIONS}"
sshcmd=( "ssh" -M "${USER}@${REMOTE_IP}" "${cmd} > ${SCRIPTPATH}/test_serverless_server_${SIZE}_$THREADS 2>&1")
$("${sshcmd[@]}")   

wait ${client_pid}

echo "Server"
tail ${SCRIPTPATH}/test_serverless_server_${SIZE}_${THREADS}
echo "Client"
tail ${SCRIPTPATH}/test_serverless_client_${SIZE}_${THREADS}

killAllProcesses

