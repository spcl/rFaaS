#!/bin/bash

usage() {
  echo "Usage: $0 [-e ssh|srun ] [ -o OUTPUT_DIR ] [-t TOOL_ARGUMENTS] node1 node2 ... nodeN" 1>&2 
}

echoerr() {
  echo "$@" 1>&2;
}

function log () {
  if [[ $verbose = 'true' ]]; then
    echo "$@"
  fi
}

output="devices"
tool_arg=""
verbose='false'
type='ssh'
while getopts "e:t:o:vh" opt; do
    case $opt in
        o) output="$OPTARG";;
        t) tool_arg="$OPTARG";;
        e) type="$OPTARG";;
        v) verbose='true';;
        h) usage
        exit 0;;
        *) usage
        exit 1;;
    esac
done

shift "$((OPTIND-1))"

log "Writing output files to $output"
mkdir -p $output


if [[ $type = 'ssh' ]]; then

  PIDS=()
  for node in "$@"
  do
    log "Connecting via SSH to $node"
    ssh $node bash -s -- ${tool_arg} < tools/device_generator.sh > ${output}/${node}.json &
    pid=$!
    PIDS+=($pid)
  done
  wait "${PIDS[@]}"

elif [[ $type = 'srun' ]]; then

  srun /bin/bash -c 'tools/device_generator.sh > '${output}'/$(hostname -s).json'

else
  echoerr "Incorrect execution type $type"
fi

