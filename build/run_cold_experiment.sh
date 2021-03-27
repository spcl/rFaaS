#!/bin/bash

trap 'echo -ne "Stop tests...\n"  && killAllProcesses && exit 1' INT

define(){ IFS='\n' read -r -d '' ${1} || true; }
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

REMOTE_IP=192.168.1.10
PORT2=9999

killAllProcesses(){
    if [ ! -z ${rpid+x} ]; then
	    echo "Killing remote processes with PID$rpid"
	    ssh ${USER}@${REMOTE_IP} "kill -9 $rpid"
    fi
}


size=1024
RX_DEPTH=512
REPETITIONS=1000
WARMUP=100
OPTIONS="--polling-mgr thread --polling-type wc --pin-threads true --max-inline-data 64"
CLIENT_OPTIONS="--max-inline-data 64"
cmd="taskset 0x3 ${SCRIPTPATH}/bin/executor_manager -a ${REMOTE_IP} -p $PORT2 -v -f ${SCRIPTPATH}/server.json"
sshcmd=( "ssh" "${USER}@${REMOTE_IP}" "nohup" "${cmd}" "> ${SCRIPTPATH}/serverless_server_$size 2>&1" "&" "echo \$!" )
echo "${sshcmd[@]}"
rpid=$("${sshcmd[@]}")   

echo "Sshed the server with PID${rpid}"
sleep 3
echo "Start client locally"
${SCRIPTPATH}/tests/cold_benchmarker -f ${SCRIPTPATH}/server.json -r 1000 --out-file out.csv -s 1024 -n empty --recv-buf-size 32 -v  #> ${SCRIPTPATH}/serverless_client_$size

echo "test is done"

sleep 4

killAllProcesses

