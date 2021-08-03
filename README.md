# serverless-rdma


## Installation

cmake params

works only on Linux!

## Configuration

use the JSON file

- max inline
- default receive buffer size
- testing

```json
{
  "devices": [
    {
      "name": IBV_DEVICE_NAME,
      "ip_address": IP_ADDRESS,
      "max_inline_data": MAX_INLINE_DATA_ON_DEVICE,
      "default_receive_buffer_size": XXXX
    }
  ]
}
```

### Testing

This file describes which devices, servers, and ports to use when executing testing

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

