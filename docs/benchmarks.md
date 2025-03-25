rFaaS comes with four benchmark applications. To set up the benchmark environment please follow the [tutorial](tutorial.md) first.

## Warm Invocations
```
<build-dir>/benchmarks/warm_benchmarker --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/libfunctions.so --executors-database executors_database.json -s <payload-size>
```

## Cold Invocations
```
<build-dir>/benchmarks/cold_benchmarker --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/libfunctions.so --executors-database executors_database.json -s <payload-size> --pause <milliseconds>
```

## Parallel Invocations
```
<build-dir>/benchmarks/parallel_invocations --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/libfunctions.so --executors-database executors_database.json -s <payload-size> --cores <number-of-cores>
```

## C++ Interface
```
<build-dir>/benchmarks/cpp_interface --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/libfunctions.so --executors-database executors_database.json -s <payload-size>
```

## RMA Functions
```
<build-dir>/benchmarks/rma --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/librma_functions.so --executors-database executors_database.json -s <payload-size> --read_size <size> --rdma_type <rdma-type> --pause <milliseconds>
```
