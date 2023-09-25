# Libfabric Networking Development

This document serves to track the development of the libfabric implementation of rFaaS, particularly for supporting more providers. Assume libfabric version 1.11.1.

# TCP
View info on the TCP provider with `fi_info --provider='tcp' -v`.

TCP docs: [here](https://ofiwg.github.io/libfabric/v1.11.1/man/fi_tcp.7.html)

The TCP provider will not work with the current architecture of rFaaS for a few reasons:
1. Atomics are not supported. This is not too big of a deal, since only accounting requires atomics, but would still require some redesigning.

2. TCP only supports `FI_EP_MSG` endpoints. rFaaS currently calls `fi_passive_ep()` which, according to [this](https://lists.openfabrics.org/pipermail/libfabric-users/2020-February/000617.html), is for `FI_EP_MSG`. In theory, if we need `FI_EP_RDM`, we can use TCP on top of RXM, which gives us `FI_EP_RDM`. In this case, the provider string is `tcp;ofi_rxm`. When using this provider, `fi_passive_ep()` fails, which is consistent with the above logic.

I got `fi_passive_ep` to work by doing the following.

I enabled all capabilities for the `tcp` provider, and used `FI_EP_MSG` as follows:
```cpp
hints->caps |= FI_MSG | FI_RMA | FI_READ | FI_WRITE | FI_RECV | FI_SEND | FI_REMOTE_READ | FI_REMOTE_WRITE | FI_LOCAL_COMM | FI_REMOTE_COMM;
hints->ep_attr->type = FI_EP_MSG;
hints->fabric_attr->prov_name = "tcp";
```

The server successfully registers endpoints with `fi_passive_ep()`.

However, now `fi_cntr_open()` fails:

The resulting error message (before the server even starts):
```cpp
Expected zero, found: -38, message Function not implemented, errno 95, message Operation not supported
```

Backtrace (note: line numbers might be slightly imprecise):
```cpp
#8  0x000055555565a574 in rdmalib::RDMAPassive::allocate (this=0x7fffffffe020) at /home/ubuntu/rfaas-libfabric-raw/rdmalib/lib/rdmalib.cpp:489
#9  0x000055555565a2e9 in rdmalib::RDMAPassive::RDMAPassive (this=0x7fffffffe020, ip="172.31.82.200", port=10000, recv_buf=32, initialize=true, max_inline_data=0)
    at /home/ubuntu/rfaas-libfabric-raw/rdmalib/lib/rdmalib.cpp:447
#10 0x00005555555c93f2 in rfaas::executor_manager::Manager::Manager (this=0x7fffffffdec0, settings=..., skip_rm=true) at /home/ubuntu/rfaas-libfabric-raw/server/executor_manager/manager.cpp:25
#11 0x00005555555659d2 in main (argc=7, argv=0x7fffffffe328) at /home/ubuntu/rfaas-libfabric-raw/server/executor_manager/cli.cpp:59
```

which is the following line in `rdmalib.cpp`:
```cpp
impl::expect_zero(fi_cntr_open(_pd, &cntr_attr, &_write_counter, nullptr));
```

We are unsure, but this is likely due to the lack of atomics support for `tcp` or `tcp;ofi_rxm`. I tried to temporarily move past this issue by disabling all counting. I did so by commenting out all atomic and counter operations, and accounting. This enabled the server to successfully listen for and process requests with the client and the executor worker. It hangs here:

In the server:
```cpp
[23:06:45:612061] [P 274006] [T 273804] [info] Child fork begins work on PID 274006, using Docker? false
[23:06:45:613248] [P 273802] [T 273804] [info] Client 1 at 172.31.82.200:10005 has executor with 274006 ID and 1 cores, time 2038 us
[23:06:45:613598] [P 273802] [T 273804] [debug] Post 1 requests to buffer at QP 0x7ffff0008110
[23:06:45:613624] [P 273802] [T 273804] [debug] Batch 0 1 to local QPN on connection 0x7ffff0007070 fid 0x7ffff0008110
[23:06:45:613647] [P 273802] [T 273804] [debug] Batched receive on connection 0x7ffff0007070 num_sge 1 sge[0].ptr 0x7ffff7fb1000 sge[0].length 36
[23:06:45:613669] [P 273802] [T 273804] [debug] Batched Post empty recv successfull on connection 0x7ffff0007070
[23:06:45:875923] [P 273802] [T 273803] [debug] [Manager-listen] Polled new rdmacm event
[23:06:45:875984] [P 273802] [T 273803] [debug] [RDMAPassive] received event: 1 in text FI_CONNREQ
[23:06:45:876018] [P 273802] [T 273803] [debug] Allocate a connection 0x7ffff0009800
[23:06:45:876042] [P 273802] [T 273803] [debug] [RDMAPassive] Connection request with ret 20
[23:06:45:876065] [P 273802] [T 273803] [debug] [RDMAPassive] Connection request with private data 70196
[23:06:45:876698] [P 273802] [T 273803] [debug] Initialize connection 0x7ffff0009800
[23:06:45:876729] [P 273802] [T 273803] [debug] [RDMAPassive] Created connection fid 0x7ffff000a8a0 qp 0x7ffff000a8a0
[23:06:45:876757] [P 273802] [T 273803] [debug] [Manager-listen] New rdmacm connection event - connection 0x7ffff0009800, status 1
[23:06:45:876780] [P 273802] [T 273803] [debug] [Manager-listen] Requested new connection 0x7ffff0009800
[23:06:45:876808] [P 273802] [T 273803] [debug] [RDMAPassive] Connection accepted at QP 0x7ffff000a8a0
[23:06:45:876857] [P 273802] [T 273803] [debug] [Manager-listen] Polled new rdmacm event
[23:06:45:876888] [P 273802] [T 273803] [debug] [RDMAPassive] received event: 2 in text FI_CONNECTED
[23:06:45:876912] [P 273802] [T 273803] [debug] [RDMAPassive] Connection is established for id 0x7ffff000a8a0, and connection 0x7ffff0009800
[23:06:45:876936] [P 273802] [T 273803] [debug] [Manager-listen] New rdmacm connection event - connection 0x7ffff0009800, status 2
[23:06:45:876958] [P 273802] [T 273803] [debug] [Manager-listen] New established connection 0x7ffff0009800
[23:06:45:876980] [P 273802] [T 273803] [debug] Executor for client 1
[23:06:45:877586] [P 273802] [T 273804] [debug] Connected executor for client 1
```

In the client:
```cpp
[23:06:45:892338] [T 274003] [debug] [Executor] Established connection to executor 1, connection 0x555555798e60
[23:06:45:892352] [T 274003] [debug] Post send to local Local QPN on connection 0x555555798e60 fid 0x555555799f00
[23:06:45:892625] [T 274003] [debug] Post send successful on connection 0x555555798e60, sges_count 1, sge[0].addr 0x7ffff7fae000, sge[0].size 17808, wr_id 0
[23:06:45:892659] [T 274003] [debug] Connected thread 1/1 and submitted function code.
[23:06:45:915663] [T 274003] [debug] Connection 0x555555798e60 Queue recv Ret 1/1 WC 0
[23:06:45:915706] [T 274003] [debug] Received buffer details for thread, id 0, addr 140737353830400, rkey 14
[New Thread 0x7ffff749a640 (LWP 274008)]
[23:06:45:919920] [T 274003] [debug] Connection 0x555555798e60 Queue send Ret 1/1 WC 0
[23:06:45:919951] [T 274003] [debug] Code submission for all threads is finished
[23:06:45:919965] [T 274003] [debug] Deallocate 17808 bytes, mr 0x55555577afd0, ptr 0x7ffff7fae000
[23:06:45:920016] [T 274003] [debug] Allocated 1016 bytes, address 0x7ffff7fb2000
[23:06:45:920046] [T 274003] [debug] Allocated 1000 bytes, address 0x7ffff7fb1000
[23:06:45:920073] [T 274003] [info] Registered 1016 bytes, mr 0x55555577afd0, address 0x7ffff7fb2000, lkey 0x55555577afd0, rkey 12
[23:06:45:920098] [T 274003] [info] Registered 1000 bytes, mr 0x55555579aa70, address 0x7ffff7fb1000, lkey 0x55555579aa70, rkey 14
[23:06:45:920122] [T 274003] [info] benchmarker remote key is 14
[23:06:45:920159] [T 274003] [info] Warmups begin
[23:06:45:920179] [T 274003] [info] Warmups completed
[23:06:45:920202] [T 274003] [debug] Submit execution 0
[23:06:45:920214] [T 274003] [debug] Invoke function 0 with invocation id 1, submission id 65536
[23:06:45:920489] [T 274003] [debug] Post write succesfull id: 2, buf size: 1016, lkey 0x55555577afd0, remote addr 140737353830400, remote rkey 14, imm data 4363686838272, connection 0x555555798e60
[23:06:45:921194] [T 274008] [info] Background thread starts waiting for events
```

In the executor:
```cpp
[23:06:45:899684] [T 274007] [debug] [RDMAActive] Connection successful to 172.31.82.200:10005
[23:06:45:899728] [T 274007] [info] Registered 1000 bytes, mr 0x7ffff00540c0, address 0x7ffff7ffa000, lkey 0x7ffff00540c0, rkey 12
[23:06:45:899742] [T 274007] [info] Registered 1016 bytes, mr 0x7ffff0054200, address 0x7ffff7fb5000, lkey 0x7ffff0054200, rkey 14
[23:06:45:899753] [T 274007] [info] Thread 0 Established connection to client!
[23:06:45:899770] [T 274007] [debug] Allocated 16 bytes, address 0x7ffff7fb3000
[23:06:45:899784] [T 274007] [info] Registered 16 bytes, mr 0x7ffff00543b0, address 0x7ffff7fb3000, lkey 0x7ffff00543b0, rkey 16
[23:06:45:899801] [T 274007] [debug] Thread 0 Sends buffer details to client! Addr 140737353830400 rkey 14
[23:06:45:899818] [T 274007] [debug] Post send to local Local QPN on connection 0x7ffff0051fe0 fid 0x7ffff0053680
[23:06:45:900073] [T 274007] [debug] Post send successful on connection 0x7ffff0051fe0, sges_count 1, sge[0].addr 0x7ffff7fb3000, sge[0].size 16, wr_id 0
[23:06:45:900092] [T 274007] [debug] Connection 0x7ffff0051fe0 Queue send Ret 1/1 WC 0
[23:06:45:900102] [T 274007] [debug] Thread 0 Sent buffer details to client!
[23:06:45:900156] [T 274007] [debug] Connection 0x7ffff0051fe0 Queue recv Ret 1/1 WC 0
[23:06:45:900311] [T 274007] [info] Thread 0 begins work with timeout -1
[23:06:45:900326] [T 274007] [debug] Thread 0 Begins hot polling
```

This is the exact same issue we are getting with sockets, and it is most likely due to the lack of support for `FI_RMA_EVENT` on these providers, since rFaaS uses `FI_REMOTE_WRITE` which requires it. See more [here](https://ofiwg.github.io/libfabric/v1.11.1/man/fi_mr.3.html). `FI_RMA_EVENT` triggers a notification when the completion queue has a new write (I think?), which makes sense why this is hanging here.

Another user had this issue back in 2015 while using psm/sockets [here](https://github.com/ofiwg/libfabric/issues/1300). Not 100% sure, but it looks like the user fixed the issue with `FI_REMOTE_WRITE_CQ`. However, none of the providers I tested have this capability, and the post was quite outdated.

A potential solution is to manually send a second message to notify that a request has been sent, but that would have large performance issues, greatly increasing latency.

3. TCP does not support `FI_RMA_EVENT`, which is quite important (see above issue)
4. TCP (and TCP with RXM (`tcp;ofi_rxm`)) does not support `FI_MR_PROV_KEY`. Sockets also sees this issue, and I discuss it more below.

# Sockets
View info on the sockets provider with `fi_info --provider='sockets' -v`.

Sockets docs: [here](https://ofiwg.github.io/libfabric/v1.11.1/man/fi_sockets.7.html).

The `sockets` provider is now deprecated, but it should still work. According to [this](https://github.com/ofiwg/libfabric#sockets), the sockets provider was not developed for speed, but for usability by machines not equipped with RDMA hardware.

## Good news

Sockets does support `FI_ATOMIC` and `FI_EP_MSG`, which is good. With sockets, we do not need to change the accounting scheme, endpoint type, or work around using counters.

Fixed issue:
1. Sockets does not support `FI_MR_PROV_KEY`. This feature is necessary to authenticate with remote buffers by supplying a key. I fixed this by trying the following:
   * Hardcoding a set of keys to be used by the server and client
   * Having the server generate a random key, and the client use that key

Another viable solution would be to have the server and client exchange keys, but since the above two worked, I did not try this. Both of these solutions worked, since we verified that the keys on the server and client matched for the appropriate buffers. Now, we get no errors. However, the server/executor hangs in a similar place as for TCP:

## Less good news

The main issue with sockets:
1. No support for `FI_RMA_EVENT` (see issue above with TCP).

Given the following hints (granted all sockets capabilities)
```cpp
hints->caps |= FI_MSG | FI_RMA | FI_TAGGED | FI_ATOMIC | FI_READ | FI_WRITE | FI_RECV | FI_SEND | FI_REMOTE_READ | FI_REMOTE_WRITE | FI_MULTI_RECV | FI_TRIGGER | FI_FENCE | FI_LOCAL_COMM | FI_REMOTE_COMM | FI_SHARED_AV | FI_RMA_EVENT | FI_SOURCE | FI_NAMED_RX_CT | FI_DIRECTED_RECV;
hints->ep_attr->type = FI_EP_MSG;
```

Using sockets hangs here on the server:
```cpp
[23:25:49:754351] [P 274899] [T 274905] [debug] Allocate a connection 0x7fffe8023570
[23:25:49:754371] [P 274899] [T 274905] [debug] [RDMAPassive] Connection request with ret 20
[23:25:49:754390] [P 274899] [T 274905] [debug] [RDMAPassive] Connection request with private data 4660
[23:25:49:754464] [P 274899] [T 274905] [debug] Initialize connection 0x7fffe8023570
[23:25:49:754799] [P 274899] [T 274905] [debug] [RDMAPassive] Created connection fid 0x7fffe8024610 qp 0x7fffe8024610
[23:25:49:754831] [P 274899] [T 274905] [debug] [Manager-listen] New rdmacm connection event - connection 0x7fffe8023570, status 1
[23:25:49:754850] [P 274899] [T 274905] [debug] [Manager-listen] Requested new connection 0x7fffe8023570
[23:25:49:759645] [P 274899] [T 274905] [debug] [RDMAPassive] Connection accepted at QP 0x7fffe8024610
[23:25:49:759688] [P 274899] [T 274905] [debug] [Manager-listen] Polled new rdmacm event
[23:25:49:759717] [P 274899] [T 274905] [debug] [RDMAPassive] received event: 2 in text FI_CONNECTED
[23:25:49:759739] [P 274899] [T 274905] [debug] [RDMAPassive] Connection is established for id 0x7fffe8024610, and connection 0x7fffe8023570
[23:25:49:759759] [P 274899] [T 274905] [debug] [Manager-listen] New rdmacm connection event - connection 0x7fffe8023570, status 2
[23:25:49:759779] [P 274899] [T 274905] [debug] [Manager-listen] New established connection 0x7fffe8023570
[23:25:49:759797] [P 274899] [T 274905] [debug] Executor for client 0
[23:25:49:763389] [P 274899] [T 274906] [debug] Connected executor for client 0
```

On the client:
```cpp
[23:25:49:819795] [T 274907] [debug] Post send to local Local QPN on connection 0x5555557f5eb0 fid 0x5555557f6f50
[23:25:49:819914] [T 274907] [debug] Post send successful on connection 0x5555557f5eb0, sges_count 1, sge[0].addr 0x7ffff7fae000, sge[0].size 17808, wr_id 0
[23:25:49:819943] [T 274907] [debug] Connected thread 1/1 and submitted function code.
[23:25:49:847642] [T 274907] [debug] Connection 0x5555557f5eb0 Queue recv Ret 1/1 WC 0
[23:25:49:847684] [T 274907] [debug] Received buffer details for thread, id 0, addr 140737353830400, rkey 14
[New Thread 0x7ffff3520640 (LWP 274924)]
[23:25:49:855919] [T 274907] [debug] Connection 0x5555557f5eb0 Queue send Ret 1/1 WC 0
[23:25:49:855963] [T 274907] [debug] Code submission for all threads is finished
[23:25:49:855985] [T 274907] [debug] Deallocate 17808 bytes, mr 0x55555579b790, ptr 0x7ffff7fae000
[23:25:49:856032] [T 274907] [debug] Allocated 1016 bytes, address 0x7ffff7fb2000
[23:25:49:856061] [T 274907] [debug] Allocated 1000 bytes, address 0x7ffff7fb1000
[23:25:49:856086] [T 274907] [info] Registered 1016 bytes, mr 0x55555579b790, address 0x7ffff7fb2000, lkey 0xc, rkey 12
[23:25:49:856108] [T 274907] [info] Registered 1000 bytes, mr 0x5555557f78c0, address 0x7ffff7fb1000, lkey 0xe, rkey 14
[23:25:49:856128] [T 274907] [info] benchmarker remote key is 14
[23:25:49:856157] [T 274907] [info] Warmups begin
[23:25:49:856174] [T 274907] [info] Warmups completed
[23:25:49:856191] [T 274907] [debug] Submit execution 0
[23:25:49:856211] [T 274907] [debug] Invoke function 0 with invocation id 1, submission id 65536
[23:25:49:859640] [T 274907] [debug] Post write succesfull id: 2, buf size: 1016, lkey 0xc, remote addr 140737353830400, remote rkey 14, imm data 4363686838272, connection 0x5555557f5eb0
[23:25:49:867642] [T 274924] [info] Background thread starts waiting for events
```

And in the executor:
```cpp
[23:25:49:827647] [T 274917] [debug] [RDMAActive] Connection successful to 172.31.82.200:10005
[23:25:49:827689] [T 274917] [info] Registered 1000 bytes, mr 0x7ffff00d5070, address 0x7ffff7ffa000, lkey 0xc, rkey 12
[23:25:49:827703] [T 274917] [info] Registered 1016 bytes, mr 0x7ffff00d51e0, address 0x7ffff7fb5000, lkey 0xe, rkey 14
[23:25:49:827715] [T 274917] [info] Thread 0 Established connection to client!
[23:25:49:827733] [T 274917] [debug] Allocated 16 bytes, address 0x7ffff7fb3000
[23:25:49:827746] [T 274917] [info] Registered 16 bytes, mr 0x7ffff00d53c0, address 0x7ffff7fb3000, lkey 0x10, rkey 16
[23:25:49:827764] [T 274917] [debug] Thread 0 Sends buffer details to client! Addr 140737353830400 rkey 14
[23:25:49:827780] [T 274917] [debug] Post send to local Local QPN on connection 0x7ffff00b4660 fid 0x7ffff00b57e0
[23:25:49:827799] [T 274917] [debug] Post send successful on connection 0x7ffff00b4660, sges_count 1, sge[0].addr 0x7ffff7fb3000, sge[0].size 16, wr_id 0
[23:25:49:848116] [T 274917] [debug] Connection 0x7ffff00b4660 Queue send Ret 1/1 WC 0
[23:25:49:848141] [T 274917] [debug] Thread 0 Sent buffer details to client!
[23:25:49:848165] [T 274917] [debug] Connection 0x7ffff00b4660 Queue recv Ret 1/1 WC 0
[23:25:49:848327] [T 274917] [info] Thread 0 begins work with timeout -1
[23:25:49:848342] [T 274917] [debug] Thread 0 Begins hot polling
```

This is basically the exact same issue with TCP. [This](https://github.com/ofiwg/libfabric/issues/1300) might also apply (same link as above).

# EFA
View info on the EFA provider with `fi_info --provider='efa' -v`.

EFA docs: [here](https://ofiwg.github.io/libfabric/v1.11.1/man/fi_efa.7.html).

EFA will not work with the current architecture of rFaaS either:
1. EFA does not support `FI_EP_MSG` (large issue). So we need to re-write rFaaS to not use `fi_passive_ep()`. Instead, we will need to use `FI_EP_RDM` endpoints. This is not trivial.
2. EFA does not support `FI_RMA_EVENT` (large issue). This is the same issue that TCP and Sockets were experiencing. Even if we did have `MSG` endpoints, we would experience the same hanging issue.
3. EFA does not support `FI_ATOMIC` (mild issue). No atomic support for accounting.

However, EFA does support `FI_MR_PROV_KEY`.

# Other Notes
 * The error message `Expected zero, found: -2, errno 28, message No space left on device` means that libfabric could not find a provider with the provided hints.

