#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <rdmalib/rdmalib.hpp>
#include "rma_functions.hpp"

extern "C" uint32_t empty(void* args, uint32_t size, void* res)
{
  std::cerr << "FUNCTION START\n";
  rmafunctions::RmaFunctionConfig* src = static_cast<rmafunctions::RmaFunctionConfig*>(args);
  rmafunctions::RmaFunctionConfig* dest = static_cast<rmafunctions::RmaFunctionConfig*>(res);
  *dest = *src;

  std::cerr << "Config: ip: " << src->client_ip_address << ", port: " << src->client_port << ", memory: " << src->rma_memory_in_bytes << std::endl;

  rdmalib::RDMAPassive _state(src->client_ip_address, src->client_port, 32, true);

  rdmalib::Buffer<char> memory_data(src->rma_memory_in_bytes);
  memory_data.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
  memset(memory_data.data(), 0, src->rma_memory_in_bytes);

  rdmalib::Buffer<char> memory_cfg(12);
  memory_cfg.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
  *reinterpret_cast<uint64_t*>(memory_cfg.data()) = memory_data.address();
  *reinterpret_cast<uint32_t*>(memory_cfg.data()+8) = memory_data.rkey();

  std::cerr << "Registered " << memory_data.address() <<  " " << memory_data.rkey() << '\n';

  int POLLING_TIMEOUT_MS = 100;
  while(true) {
    bool result = _state.nonblocking_poll_events(POLLING_TIMEOUT_MS);
    auto [conn, conn_status] = _state.poll_events();

    if(conn == nullptr){
      std::cerr << "Failed connection creation" << std::endl;
      continue;
    }

    if(conn_status == rdmalib::ConnectionStatus::DISCONNECTED) {
      // FIXME: handle disconnect
      std::cerr << "[Manager-listen] Disconnection on connection: ";
      break;
    }
    else if(conn_status == rdmalib::ConnectionStatus::REQUESTED) {
      std::cerr << "Polled, accept!" << std::endl;
      _state.accept(conn);
      conn->post_send(memory_cfg, 0, 0);
      conn->poll_wc(rdmalib::QueueType::SEND, true, 1);
    }
  }

  return size;
}

