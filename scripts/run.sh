#!/bin/bash

cmake --build .

cmd=''

if [[ "$1" == "server" ]]; then
    cmd="./bin/executor_manager -c config/executor_manager.json --device-database config/devices.json --skip-resource-manager"
elif [[ "$1" == "bench" ]]; then
    cmd="./benchmarks/warm_benchmarker --config config/benchmark.json --device-database config/devices.json --name empty --functions ./examples/libfunctions.so --executors-database config/executors_database.json -s 1000"
fi

if [[ "$2" == "debug" ]]; then
    cmd="gdb --args $cmd"
fi

final="PATH=$PATH:bin/ $cmd"
eval "$final"

