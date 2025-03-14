#!/bin/bash

usage() {
  echo "Usage: $0 [ -d DEVICE ] [ -p PORT] [ -o OUTPUT ]" 1>&2 
}

echoerr() {
  echo "$@" 1>&2;
}

function log () {
  if [[ $verbose = 'true' ]]; then
    echo "$@"
  fi
}

get_device() {
  device=$1
  port=$2
  netdev=$3
  addr=$(ip -j address show dev $netdev | jq -r '.[0].addr_info[] | select(.family=="inet") | .local')

  # case-insensitive comparison
  if [[ -z "${addr}" || "${addr,,}" = "null" ]]; then
    echoerr "Could not determine the address of device $device"
  else
    output_json=$(jq --arg netdev $netdev --arg device $device --arg addr $addr --argjson port $port '.devices += [{"name": $device, "netdev": $netdev, "ip_address": $addr, "port": $port, "default_receive_buffer_size": 32, "max_inline_data": 0}]' <<< ${output_json})
  fi
}

output=""
verbose='false'
port=0
while getopts "d:o:p:hv" opt; do
    case $opt in
        d) devices+=("$OPTARG");;
        p) port="$OPTARG";;
        o) output="$OPTARG";;
        v) verbose='true';;
        h) usage
        exit 0;;
        *) usage
        exit 1;;
    esac
done

output_json=$(jq --null-input '{"devices": []}')

rdma_devices=$(rdma link)
if [[ -n $devices ]]; then
  log "Querying the following devices: ${devices[@]}"
  pattern=$(IFS='|'; echo "${devices[*]}")
  rdma_devices=$(echo "$rdma_devices" | 
  awk -v pat="$pattern" '{
    split($2, a, "/");
    if(a[1] ~ "^"pat"$") print
  }')
else
  log "Querying devices from the rdma command."
fi

while read -r _ device _ state _ _ _ netdev; do
  if [ "$state" = "ACTIVE" ]; then
    device=${device%%/*}
    log "Process $device"
    get_device "$device" "$port" "$netdev"
  fi
done <<< "${rdma_devices}"

if [[ ! -z $output ]]; then
  log "Writing to $output"
  # run jq to pretify
  jq <<< ${output_json} > $output
else
  jq . <<< ${output_json}
fi

