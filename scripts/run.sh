#!/bin/bash

cmake --build .
cd config/

if [[ "$1" -eq "server" ]]; then
    PATH=$PATH:../bin/ ../bin/executor_manager -c executor_manager.json --device-database devices.json --skip-resource-manager
elif [[ "$1" -eq "bench" ]]; then
    PATH=$PATH:../bin/ ../benchmarks/warm_benchmarker --config benchmark.json --device-database devices.json --name empty --functions ../examples/libfunctions.so --executors-database executors_database.json -s 1000
fi

