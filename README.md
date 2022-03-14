# rFaaS: RDMA-Enabled FaaS Platform for Serverless High-Performance Computing

**A high-performance FaaS platform with RDMA acceleration for function invocations.**

![License](https://img.shields.io/github/license/spcl/rfaas)
![GitHub issues](https://img.shields.io/github/issues/spcl/serverless-benchmarks)
![GitHub pull requests](https://img.shields.io/github/issues-pr/spcl/serverless-benchmarks)

[<img alt="rFaaS vs HPC vs FaaS" src="docs/systems_comparison.png" height="200" align="right" title="rFaaS vs HPC vs FaaS"/>](docs/systems_comparison.png)
The cloud paradigm Function-as-a-Service (FaaS) provides an ability to execute stateless and fine-grained functions on elastic and ephemeral resources. However, serverless struggles to achieve the performance needed in high-performance computing: slow invocations, low network bandwidth, and the overheads of the FaaS management system make it difficult to incorporate serverless functions when every millisecond counts. Therefore, we decided to combine the best of both worlds: elasticity of FaaS and high-performance of cluster batch systems. We built a new FaaS platform with RDMA-accelerated network transport.

rFaaS is a serverless platform redesigned to support high-performance and low-latency invocations with a direct RDMA connection. 
In rFaaS, the centralized schedulers and API gateway are replaced with a decentralized allocation mechanism. Instead of using a traditional cloud trigger, HPC applications query executor servers, obtain resource allocation and establish RDMA connections to remote workers. Every function is invoked by writing input data directly to the memory of the worker. This allows us to achieve a single-digit microsecond hot invocation latency - hot invocations add less than 350 nanoseconds overhead on top of the fastest available network transmission.

To use rFaaS, please read the documentation on [software and hardware requirements](#requirements), [installation instructions](#installation), and [the basic example of using rFaaS](#usage). rFaaS comes with a set of [benchmark](docs/benchmarks.md) applications and [tests](docs/testing). We provide an extended set of [C++ serverless functions](docs/examples.md), including multimedia and ML inference examples from [the serverless benchmarking suite SeBS](https://github.com/spcl/serverless-benchmarks). Finally, you can find more details about rFaaS in the documentation on the [system](docs/system.md) and the [client rFaaS library](docs/client_library.md).

Do you have further questions not answered by our documentation? Did you encounter troubles with installing and using rFaaS? Or do you want to use rFaaS in your work and you need new features? Feel free to reach us through GitHub issues or by writing to <marcin.copik@inf.ethz.ch>.

### Paper

When using rFaaS, please cite our [arXiv paper preprint](https://arxiv.org/abs/2106.13859), and you can
find more details about research work [in this paper summary](mcopik.github.io/projects/rfaas/).
You can cite our software repository as well, using the citation button on the right.

```
@misc{copik2021rfaas,
   title={RFaaS: RDMA-Enabled FaaS Platform for Serverless High-Performance Computing}, 
   author={Marcin Copik and Konstantin Taranov and Alexandru Calotoiu and Torsten Hoefler},
   year={2021},
   eprint={2106.13859},
   archivePrefix={arXiv},
   primaryClass={cs.DC}
}
```

## Requirements

**Hardware** `rFaaS` supports SoftROCE and RoCE RDMA NICs with the help of `ibverbs`.
Evaluation and testing with IB fabric is currently in progress.

In future versions, we plan for `rFaaS` to support Cray interconnect through `libfabric` and
its `ugni` provider.

**Software** Currently, `rFaaS` works only on Linux systems as we realy heavily on POSIX interfaces. We require the following libraries and tools:

- CMake >= 3.11.
- C++ compiler with C++17 support.
- `libibverbs` with headers installed.
- `librdmacm` with headers installed.
- [pistache](https://github.com/pistacheio/pistache) - HTTP and REST framework.

Furthermore, we fetch and build the following dependencies during CMake build - unless
they are found already in the system.

- [spdlog](https://github.com/gabime/spdlog) 1.8
- [cereal](https://uscilab.github.io/cereal/) 1.3
- [cxxopts](https://github.com/jarro2783/cxxopts) 
- [googletest](https://github.com/google/googletest)

**Containers**
`rFaaS` supports two types of function executors - a bare-metal process and a Docker container. For Docker, we use the SR-IOV plugin from Mellanox to run Docker-based function executors with virtual NIC device functions. Please follow [Mellanox documentation and instructions](https://community.mellanox.com/s/article/Docker-RDMA-SRIOV-Networking-with-ConnectX4-ConnectX5-ConnectX6) to install and configure the plugin.
`rFaaS` expects that `docker_rdma_sriov` binary is available in `PATH`.

In future versions, we plan to support Singularity containers and offer a simpler, but less secure Docker networking.

## Installation

To build rFaaS, run the following CMake configuration:

```bash
cmake -DCMAKE_CXX_COMPILER=<your-cxx-compiler> -DCMAKE_BUILD_TYPE=Release <source-dir>
cmake --build .
```

To enable more verbose logging, change the CMake configuration parameter to: `-DCMAKE_BUILD_TYPE=Debug`.

The CMake installation has the following optional configuration parameters.

| Arguments                                                            	|                                              		|
|-------------------------------------------------------------------|----------------------------------------------|
| <i>WITH_EXAMPLES</i>                                       	| **EXPERIMENTAL** Build additional examples ([see examples subsection](docs/examples.md) for details on additional dependencies).              						|
| <i>WITH_TESTING</i>                                        	| **EXPERIMENTAL** Enable testing - requires providing device database and testing configuration (see below). See [testing](#testing) subsection for details.	|
| <i>DEVICES_CONFIG</i>                                         | File path for the JSON device configuration. |
| <i>TESTING_CONFIG</i>                                         | File path for the JSON device configuration. |
| <i>CXXOPTS_PATH</i>                                         	 | Path to an existing installation of the `cxxopts` library; disables the automatic fetch and build of the library. |
| <i>SPDLOG_PATH</i>                                         	 | Path to an existing installation of the `spdlog` library; disables the automatic fetch and build of the library. |
| <i>LIBRDMACM_PATH</i>                                        | Path to a installation directory of the `librdmacm` library. |

## Usage

In this section, we demonstrate how to use rFaaS executors with a simple function.
We show how to configure a database of RDMA devices, how to launch an executor manager
capable of launching serverless executors, and how to use one of our benchmarking applications
to allocate an executor and submit function invocations over the RDMA network.

A resource manager is an integral component of the system, as it provides executors with
global management of billing and it distributes data on active executor servers to clients.
Here, we skip the deployment of resource manager for simplicity.
On small deployments with just few executor servers, we can bypass this step.

For an in-depth analysis of each component and their configuration, please look at [the system documentation](docs/system.md).

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

In future, we plan for rFaaS to include a script for automatic generation of the database.

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

First, we need to configure the executor. An example of a configuration is available in `config/executor_manager.json`
and it needs to be extended with device and port selection.

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

To start an instance of the executor manager, we use the following command:

```
PATH=<build-dir>/bin:$PATH <build-dir>/bin/executor_manager -c <path-to-cfg.json> --device-database <path-to-dev-db.json> --skip-resource-manager
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
            "port": 10006,
            "cores": 1,
            "address": "192.168.0.21"
        }
    ]
}
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
    "rdma_device_port": 0,
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

To start a benchmark instance with the `name` functions from `examples/libfunctions.so`,
we use the following command:

```
<build-dir>/benchmarks/warm_benchmarker --config <benchmark-cfg>.json --device-database <devices>.json --name empty --functions examples/libfunctions.so --executors-database <executors>.json -s <payload-size>
```

We should see the following output:

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

## Authors

* [Marcin Copik (ETH Zurich)](https://github.com/mcopik/) - main author.
* [Konstantin Taranov (ETH Zurich)](https://github.com/TaranovK) - consultation and troubleshooting of RDMA issues.

