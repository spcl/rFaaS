
In this section, we demonstrate how to use rFaaS executors with a simple function.
We show how to configure a database of RDMA devices, how to launch an executor manager
capable of launching serverless executors, and how to use one of our benchmarking applications
to allocate an executor and submit function invocations over the RDMA network.

A resource manager is an integral component of the system, as it provides executors with
global billing management and distributes data on active executor servers to clients.
On small deployments with just a few executor servers, we can bypass this step.
Here, we begin with a simple deployment that uses executor manager only,
and then show a full deployment with three components: resource manager, executor manager, and rFaaS client.

### RDMA Configuration

rFaaS currently supports InfiniBand and RoCE devices through the ibverbs library.
If you do not own such a device, you can still emulate it on a regular Ethernet network 
with [SoftROCE](https://github.com/SoftRoCE). This kernel driver allows creating an emulated
RDMA device on top of a regular network. Of course, it won't be able to achieve the same performance
as a regular RDMA device but it will implement a similar set of functionalities.

The installation of SoftROCE should be straightforward on most modern Linux distributions,
and it should not be necessary to manually compile and install kernel modules.

For example, on Ubuntu-based distributions, you need to install `rdma-core`, `ibverbs-utils`.
Then, you can add a virtual RDMA device with the following command:

```
sudo rdma link add test type rxe netdev <netdev>
```

where `netdev` is the name of your Ethernet device used for emulation. You can check the device
has been created with the following command:

> [!WARNING]
> Do NOT use the loopback (lo) device / localhost address (127.0.0.1). Add the virtual RDMA device to the normal ethernet interface (wired or WiFi).

```
ibv_devices

    device                 node GUID
    ------              ----------------
    test                067bcbfffeb6f9f6

```

To fully check the configuration works, we can run a simple performance
test by using tools provided in the package: `perftest`. In one shell, open:

```
ib_write_bw
```

And in the second one, start:

```
ib_write_bw <ip>
```

Replace `<ip>` with the IP address of selected net device.
You should see the following output:

```
---------------------------------------------------------------------------------------
 #bytes     #iterations    BW peak[MB/sec]    BW average[MB/sec]   MsgRate[Mpps]
Conflicting CPU frequency values detected: 3902.477000 != 400.000000. CPU Frequency is not max.
 65536      5000             419.30             361.10 		   0.005778
---------------------------------------------------------------------------------------
```

Another tool used that can be used is `rping` available in `rmadcm-utils` package:

```
rping -s -a <ip-address> -v

rping -c -a <ip-address> -v
```

### Setup

We assume that rFaaS executor will be executed on one server, using the RDMA device `server_device` and the network address `server_ip`.
Then, we're going to deploy the benchmarker on another machine, using the RDMA device `benchmark_device` and the network address `benchmark_ip`.

These four variables will be used when modifying JSON config files. If only a single machine is used, then both pairs can point to the same device using the same
network address. However, make sure that each component uses a different port in such deployment!

### Device Database

Each system component uses a simple JSON data structure to store the configuration of available RDMA devices.
The database simplifies the command-line interface of each system component, as it's no longer
necessary to specify all device properties in each executable - users need to provide just the
device name and optionally specify the network port.

To generate the database automatically, we can use the helper script `tools/device_generator.sh > devices.json`.

Alternatively, an example of configuration is available in `config/devices.json`.
Here we need to only specify the device name as it is visible when running the `ibv_devices`
tool, the IP address of the interface associated with the device, and default port selection for
the device.
We can use default values for the maximal size of inline messages and the receive buffer size.

```json
{
  "devices": [
    {
      "name": "IBV_DEVICE_NAME",
      "ip_address": "IP_ADDRESS",
      "port": <PORT_NUMBER>,
      "max_inline_data": 0,
      "default_receive_buffer_size": 32
    }
  ]
}
```

> [!WARNING]  
> When working with multiple servers and a shared filesystem, we recommend including system name in the filename, e.g., `devices_$(hostname).json`. This way, you can avoid errors caused by accidentally providing wrong device configuration. These errors manifest themselves usually with a failed assertion early in the allocation of `RDMAPassive` or `RDMAActive`.

> [!NOTE]  
> Data inlining improves the performance of small messages, but it is not supported by every RDMA provider.
> In particular, it does not work on the SoftROCE emulation. The default behavior is to disable this feature with value "0".

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
We provide a simple example in `example/functions.cpp`:

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
and it needs to be extended with the device selection.

```json
{
  "config": {
    "rdma_device": "<rdma-device>",
    "rdma_device_port": 10005,
    "node_name": "exec-mgr-node",
    "resource_manager_address": "",
    "resource_manager_port": 0,
    "resource_manager_secret": 42,
    "rdma-sleep": true
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
jq --arg device "<your-device>" '.config.rdma_device = $device' <src-dir>/config/executor_manager.json > executor_manager.json
```

To start an instance of the executor manager, we use the following command:

```
PATH=<build-dir>/bin:$PATH <build-dir>/bin/executor_manager -c executor_manager.json --device-database server_devices.json --skip-resource-manager
```

> [!WARNING]  
> **IMPORTANT** The environment variable `PATH` must include the directory `<build-dir>/bin`. This is caused by executor manager using `fork` and `execvp` to start a new executor process.

After starting the manager, you should see the output similar to this:

```console
[13:12:31:629452] [P 425702] [T 425702] [info] Executing rFaaS executor manager! 
[13:12:31:634632] [P 425702] [T 425702] [info] Listening on device rocep61s0, port 10005
[13:12:31:634674] [P 425702] [T 425702] [info] Begin listening at 192.168.0.21:10005 and processing events!
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
      "node": "exec-mgr-node",
      "address": "<exec-mgr-address>"
      "port": 10005,
      "cores": 1,
      "memory": 512
    }
  ]
}
```

We can generate this using the following command:

```
jq --arg address "<exec-mgr-device-address>" '.executors[0].address = $address' <src-dir>/config/executors_database.json > executors_database.json
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
    "rdma_device_port": 10006,
    "resource_manager_address": "",
    "resource_manager_port": 0
  },
  "benchmark": {
    "pin_threads": false,
    "repetitions": 100,
    "warmup_repetitions": 0,
    "numcores": 1,
    "memory": 256,
    "hot_timeout": -1
  }
}
```

We generate the configuration using the following command:

```
jq --arg device "<client-rdma-device>" '.config.rdma_device = $device' <src-dir>/config/benchmark.json > benchmark.json
```

To start a benchmark instance with the `name` functions from `examples/libfunctions.so`,
we use the following command:

```
<build-dir>/benchmarks/warm_benchmarker --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/libfunctions.so --executors-database executors_database.json -s <payload-size>
```

We should see the following output for payload of size 1:

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

For details about this and other benchmarks, please take a look [at the documentation](benchmarks.md).

> [!NOTE]  
> If you observe failed assertions or you applications hang, first check if all devices and IP addresses are correct.

### Using Resource Manager

For large deployments, we deploy the resource manager to control leases.

An example of a configuration is available in `config/resource_manager.json`
and it needs to be extended with device selection.
By default, we assume that resource manager uses port 10000 for RDMA connections,
and port 5000 for the HTTP server.

```json
{
  "config": {
    "rdma_device": "",
    "rdma_device_port": 10000,
    "http_network_address": "0.0.0.0",
    "http_network_port": 5000,
    "rdma-threads": 1,
    "rdma-secret": 42,
    "rdma-sleep": true
  }
}
```

We can use the following command to generate the configuration:

```
jq --arg device "<res-mgr-rdma-device>" '.config.rdma_device = $device' <src-dir>/config/resource_manager.json > resource_manager.json
```

To start an instance of the resource manager, we use the following command:

```
<build-dir>/bin/resource_manager -c resource_manager.json --device-database server_devices.json -i executors_database.json
```

Here, we populate the database of all executors by providing a JSON list generated previously: `-i executors_database.json`.
Alternatively, we can use the HTTP interface designed for integration with batch managers, and send a POST request that adds a new executor:

```
curl http://127.0.0.1:5000/add\?node\=exec-mgr-node -X POST -d '{"ip_address": "192.168.0.29", "port": 10005, "cores": 1, "memory": 512}'
```

Then, we only need to modify the configuration of `executor_manager.json` and `benchmark.json` to add the IP address and port of resource manager.
For executor manager, we remove the `--skip-resource-manager` flag. For benchmarker, we remove the `--executors-database executors_database.json`
parameter, as all executor data will now be handled by the resource manager.

```
jq --arg device "<client-device>" --arg addr "<res-mgr-address>" '.config.rdma_device = $device | .config.resource_manager_address = $addr' <src-dir>//config/benchmark.json > benchmark.json
jq --arg device "<client-device>" --arg addr "<res-mgr-address>" '.config.rdma_device = $device | .config.resource_manager_address = $addr' <src-dir>//config/executor_manager.json > executor_manager.json
```

Then start the executor manager:

```
PATH=<build-dir>/bin:$PATH <build-dir>/bin/executor_manager -c executor_manager.json --device-database server_devices.json
```
And finally the benchmark:
```
<build-dir>/benchmarks/warm_benchmarker --config benchmark.json --device-database benchmark_devices.json --name empty --functions <build-dir>/examples/libfunctions.so -s <payload-size>
```
