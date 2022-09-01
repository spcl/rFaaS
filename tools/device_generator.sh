#!/bin/bash

usage() {
  echo "Usage: $0 [ -d DEVICE ] [ -o OUTPUT ]" 1>&2 
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
  addr=$(ip -j address show dev $device | jq -r '.[0].addr_info[] | select(.family=="inet") | .local')

  # case-insensitive comparison
  if [[ -z "${addr}" || "${addr,,}" = "null" ]]; then
    echoerr "Could not determine the address of device $device"
  else
    jq --arg device $device --arg addr $addr '.devices += [{"name": $device, "ip_address": $addr, "port": 0, "default_receiver_buffer_size": 32, "max_inline_data": 0}]' "$output" > "$output".tmp
    mv "$output".tmp "$output"
  fi
}

output="devices.json"
verbose='false'
while getopts "d:o:hv" opt; do
    case $opt in
        d) devices+=("$OPTARG");;
        o) output="$OPTARG";;
        v) verbose='true';;
        h) usage
        exit 0;;
        *) usage
        exit 1;;
    esac
done

jq --null-input '{"devices": []}' > "$output"
log "Writing to $output"

if [[ -n $devices ]]; then
  log "Querying the following devices: ${devices[@]}"
  for netdev in "${devices[@]}"
  do
    log "Process $netdev"
    get_device "$netdev" 
  done
else
  log "Querying devices from the rdma command."
  rdma_devices=$(rdma link)

  while read -r _ _ _ state _ _ _ netdev; do
    if [ "$state" = "ACTIVE" ]; then
      log "Process $netdev"
      get_device "$netdev" 
    fi
  done <<< "${rdma_devices}"

fi


