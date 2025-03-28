# Benchmarks
rFaaS comes with example and benchmark applications. To set up the benchmark environment please follow the [tutorial](tutorial.md) first.
* The benchmark binaries are in the `<build-dir>/benchmarks/` directory
* The function libraries are in the `<build-dir>/examples/` directory
* Use option `--help` to see all cli arguments for the applications
* Some applications have specific options; see examples below

## C++ Interface Example
See example code `benchmarks/cpp_interface.cpp` on how to invoke rFaaS functions in C++ code. Examples for functions can be found in the `examples/` directory.
Example application run:
```
<build-dir>/benchmarks/cpp_interface --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/libfunctions.so --executors-database executors_database.json -s 1
```

## Warm Invocations Benchmark
Benchmark for warm function invocations.
* App: `warm_benchmarker`
* Function: `libfunctions.so`
* Options: 
    * `-s <size>` for function invocation payload size in bytes
    * `--output-stats <filename>` optional to output measurements as csv

## Cold Invocations Benchmark
Benchmark for cold function invocations.
* App: `cold_benchmarker`
* Function: `libfunctions.so`
* Options: 
    * `-s <size>` for function invocation payload size in bytes
    * `--output-stats <filename>` optional to output measurements as csv
    * `--pause <milliseconds>` to set sleep time between iterations

## Parallel Invocations Benchmark
Benchmark for warm parallel function invocations.
* App: `parallel_invocations`
* Function: `libfunctions.so`
* Options: 
    * `-s <size>` for function invocation payload size in bytes
    * `--output-stats <filename>` optional to output measurements as csv
    * `--cores <number-of-cores>` for the amount of parallel functions to invoke


## RMA Function Benchmark
Benchmark for reads/writes to remote function memory. The client application reads or writes the payload to the memory served by the rma function.
* App: `rma`
* Function: `librma_functions.so`
* Options: 
    * `-s <size>` for payload size in bytes
    * `--output-stats <filename>` optional to output measurements as csv
    * `--rma_mode [0 | 1]` for reading (0) or writing (1) the payload
    * `--rma_memory <size>` for size of the remote memory in bytes
    * `--pause <milliseconds>` for sleep time between iterations

Example run:
```
<build-dir>/benchmarks/rma --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/librma_functions.so --executors-database executors_database.json -s 1 --rma_mode 0 --rma_memory 1024 --pause 10
```