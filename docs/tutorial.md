
In this section, we demonstrate how to use rFaaS executors with a simple function.
We show how to configure a database of RDMA devices, how to launch an executor manager
capable of launching serverless executors, and how to use one of our benchmarking applications
to allocate an executor and submit function invocations over the RDMA network.

A resource manager is an integral component of the system, as it provides executors with
global management of billing and it distributes data on active executor servers to clients.
Here, we skip the deployment of resource manager for simplicity.
On small deployments with just few executor servers, we can bypass this step.

### Setup

We assume that rFaaS executor will be executed on one server, using the RDMA device `server_device`
and the network address `server_ip`.
Then, we're going to deploy the benchmarker on another machine, using the RDMA device `benchmark_device` and the netwrok address `benchmark_ip`.

These four variables will be used when modifying JSON config files.
If only a single machine is used, then both pairs can point to the same device using the same
network address.

### Device Database

Each system component uses a simple JSON data structure to store the configuration of available
RDMA devices.
The database simplifies the command-line interface of each system component, as it's no longer
necessary to specify all device properties in each executable - users need to provide just the
device name and optionally specify the network port.

An example of configuration is available in `config/devices.json`.
Here we need to only specify the device name as it is visible when running the `ibv_devices`
tool, the IP address of the interface associated with the device, and default port selection for
the device.
We can use default values for maximal size of inline messaged and the receive buffer size.

```json
{
  "devices": [
    {
      "name": IBV_DEVICE_NAME,
      "ip_address": IP_ADDRESS,
      "port": PORT,
      "max_inline_data": 0,
      "default_receive_buffer_size": 32
    }
  ]
}
```

To defines these automatically, we can use a jq one-liner:

```
jq --arg device "$server_device" --arg address "$server_ip" '.devices[0].name = $device | .devices[0].ip_address = $address' <src-dir>/config/devices.json > server_devices.json
jq --arg device "$client_device" --arg address "$client_ip" '.devices[0].name = $device | .devices[0].ip_address = $address' <src-dir>/config/devices.json > client_devices.json
```

### rFaaS function

`rFaaS` functions behave exactly like regular serverless functions, except for their
C native interface:

```c++
extern "C" uint32_t func_name(void* args, uint32_t size, void* res)
```

The first parameter `args` points to a memory buffer with input data, and `size` contains
the number of bytes sent. The function writes the output to the memory buffer of size `res`
and the return value of the function is the number of bytes returned.


`rFaaS` expects to receive a shared library with the function.
We provide an simple example in `example/functions.cpp`:

```c++
extern "C" uint32_t empty(void* args, uint32_t size, void* res)
{
  int* src = static_cast<int*>(args), *dest = static_cast<int*>(res);
  *dest = *src;
  return size;
}
```

The examples are automatically built and the shared library `libfunctions.so` can be found
in `<build-dir>/examples`.

### Executor Manager

This lightweight allocator is responsible for accepting connections from clients,
allocating function executors, and measuring costs associated with resource consumption.

First, we need to configure the executor.
An example of a configuration is available in `config/executor_manager.json`
and it needs to be extended with device.

```json
{
  "config": {
    "rdma_device": "<rdma-device>",
    "rdma_device_port": <device-port>,
    "resource_manager_address": "",
    "resource_manager_port": 0,
    "resource_manager_secret": 0
  },
  "executor": {
    "use_docker": false,
    "repetitions": 100,
    "warmup_iters": 0,
    "pin_threads": false
  }
}
```

We can use the following command:

```
jq --arg device "$server_device" '.config.rdma_device = $device' <src-dir>/config/executor_manager.json > executor_manager.json
```

To start an instance of the executor manager, we use the following command:

```
PATH=<build-dir>/bin:$PATH <build-dir>/bin/executor_manager -c executor_manager.json --device-database server_devices.json --skip-resource-manager
```

**IMPORTANT** The environment variable `PATH` must include the directory `<build-dir>/bin`.
This is caused by executor manager using `fork` and `execvp` to start a new executor process.

After starting the manager, you should see the output similar to this:

```console
[13:12:31:629452] [P 425702] [T 425702] [info] Executing rFaaS executor manager! 
[13:12:31:634632] [P 425702] [T 425702] [info] Listening on device rocep61s0, port 10006
[13:12:31:634674] [P 425702] [T 425702] [info] Begin listening at 192.168.0.21:10006 and processing events!
```

### Benchmark Example

Finally, with an executor manager running, we can launch a client that sends function
invocations. Since we skip resource manager in previous steps, we provide the list of 
available executor managers in the JSON database.
An example of configuration is available in `config/executors_database.json`.
In our case, it looks as follows - see that IP address and port match the executor manager
configuration from the previous step:

```json
{
    "executors": [
        {
            "port": 10000,
            "cores": 1,
            "address": "<exec-mgr-address>"
        }
    ]
}
```

We can generate this using the following command:

```
jq --arg address "$server_ip" '.executors[0].address = $address' <src-dir>/config/executors_database.json > executors_database.json
```

To invoke functions, we use a benchmark application that evaluates warm and hot invocations.
Then, we need to configure the benchmark application.
An example of a configuration is available in `config/benchmark.json`
and it needs to be extended with device and port selection.
Benchmark settings allow to change the number of repetitions and the hot polling timeout:
`-1` forces to always execute hot invocations, `0` disables hot polling, and any positive
value describes the hot polling timeout in milliseconds.

```json
{
  "config": {
    "rdma_device": "",
    "rdma_device_port": 10005,
    "resource_manager_address": "",
    "resource_manager_port": 0
  },
  "benchmark": {
    "pin_threads": false,
    "repetitions": 100,
    "warmup_repetitions": 0,
    "numcores": 1,
    "hot_timeout": -1
  }
}
```

We generate the configuration using the following command:

```
jq --arg device "$client_device" '.config.rdma_device = $device' ../repo2/config/benchmark.json > benchmark.json
```

To start a benchmark instance with the `name` functions from `examples/libfunctions.so`,
we use the following command:

```
<build-dir>/benchmarks/warm_benchmarker --config benchmark.json --device-database client_devices.json --name empty --functions <build-dir>/examples/libfunctions.so --executors-database executors_database.json -s <payload-size>
```

We should see the following output for payload of size1:

```console
[14:08:33:759206] [T 431516] [info] Executing serverless-rdma test warm_benchmarker! 
[14:08:33:760560] [T 431516] [info] Listening on device rocep61s0, port 10008 
[14:08:33:770880] [T 431525] [info] Background thread starts waiting for events 
[14:08:33:770893] [T 431516] [info] Warmups begin 
[14:08:33:770902] [T 431516] [info] Warmups completed 
[14:08:33:771023] [T 431516] [info] Executed 20 repetitions, avg 5.704350000000001 usec/iter, median 5.163 
[14:08:33:870995] [T 431525] [info] Background thread stops waiting for events 
Data: 1 
```

For details about this and other benchmarks, please take a look [at the documentation](docs/benchmarks.md).

