
**EXPERIMENTAL** Testing framework is a work in progress and might not work as expected at the moment.
Testing rFaaS functionalities is a rather complex task given the multiple components and the
RDMA setup.

This file describes which devices, servers, and ports to use when executing testing:

```json
{
  "test_executor": {
    "device": DEV_NAME,
    "port": PORT
  },
  "rfaas_server": {
    "remote_hostname": REMOTE_HOSTNAME
    "device_ip": REMOTE_IP_ADDR,
    "port": REMOTE_PORT
  }
}
```
