#!/bin/bash

trap 'echo -ne "Stop tests...\n"  && killAllProcesses && exit 1' INT

RHOME=$(pwd)
echo $RHOME

REMOTE_IP=148.187.105.13
PORT2=10001

LOCAL_IP=148.187.105.11
PORT1=10002

killAllProcesses(){
	  ssh ${USER}@${REMOTE_IP} "killall -s 9 executor_manager"
    if [ ! -z ${rpid+x} ]; then
	    echo "Killing remote processes with PID$rpid"
	    #ssh ${USER}@${REMOTE_IP} "kill -9 $rpid"
    fi
}

killAllProcesses

size=1024
RX_DEPTH=512
REPETITIONS=1000
WARMUP=100
OPTIONS="--polling-mgr thread --polling-type wc --pin-threads true --max-inline-data 64"
CLIENT_OPTIONS="--max-inline-data 64"
cmd="PATH=$RHOME/bin/:$PATH ${RHOME}/bin/executor_manager -a ${REMOTE_IP} -p $PORT2 -v -f -r 1 -x 32 --warmup-iters 0 --max-inline-data 128 ${RHOME}/server.json"
#sshcmd=( "ssh" "${USER}@${REMOTE_IP}" "nohup" "${cmd}" "> ${RHOME}/serverless_server_$size 2>&1" "&" "echo \$!" )
#echo "${sshcmd[@]}"
#rpid=$("${sshcmd[@]}")   
ssh -f -M ${USER}@${REMOTE_IP} "${cmd} > ${RHOME}/serverless_server_$size 2>&1"

echo "Sshed the server with PID${rpid}"
sleep 1
echo "Start client locally"
${RHOME}/tests/cold_benchmarker -a ${LOCAL_IP} -p ${PORT1} -f ${RHOME}/servers.json -r 1000 --out-file out.csv -s 32 --fname empty --recv-buf-size 32 -v  --fname empty --flib ${RHOME}/examples/libfunctions.so --max-inline-data 128 -c 1 --hot-timeout -1

echo "test is done"

sleep 4

killAllProcesses

