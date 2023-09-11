# Running with Docker

To use the Docker execution environment or to run a private Docker registry, some configuration is necessary.

## Using the Docker Execution Environment
rFaaS supports running functions within a Docker container. The Docker image in `scripts/docker/rfaas-base.Dockerfile` is capable of running the functions provided by `example/`, and any other functions that don't require extra dependencies. This Dockerfile serves as a good base to execute your own custom functions.

To use rFaaS with Docker, consult the example configuration in `config/executor_manager.json`. Set the `sandbox_type` parameter to `"docker"`. Then, configure the Docker-specific parameters under `"key": "docker"`. Here is an example:

```json
{
  "key": "docker",
  "value": {
    "index": 0,
    "data": {
      "image": "rfaas-registry/rfaas-base",
      "network": "mynet",
      "ip": "172.31.82.202",
      "volume": "/path/to/rfaas/bin/",
      "registry_ip": "172.31.82.200",
      "registry_port": 5000
    }
  }
}
```

Note that the `"volume"` field must point to a directory containing the `executor` binary built by rFaaS.

## Local Registry
A script to handle starting a private Docker registry is provided in `scripts/docker/run_registry.sh`. Running a Docker registry requires a mount point, which 
ust be configured by the `REGISTRY_LOCATION` parameter in `scripts/docker/registry.yaml`. You can place this mount anywhere on the host machine.

