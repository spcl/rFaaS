
#include <cstdint>
#include <iostream>

#include <unistd.h>

#include <rdmalib/rdmalib.hpp>

extern "C" uint32_t empty(void* args, uint32_t size, void* res)
{
  int* src = static_cast<int*>(args), *dest = static_cast<int*>(res);
  *dest = *src;
  rdmalib::RDMAPassive _state("148.187.105.11", 20000, 32, true);

  int buf_size = 1024*1024*1024;
  rdmalib::Buffer<char> memory_data(buf_size);
  memset(memory_data.data(), 0, buf_size);

  rdmalib::Buffer<char> memory_cfg(12);
  memory_data.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
  memory_cfg.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
  *reinterpret_cast<uint64_t*>(memory_cfg.data()) = memory_data.address();
  *reinterpret_cast<uint32_t*>(memory_cfg.data()+8) = memory_data.rkey();

  std::cout << "Registered " << memory_data.address() <<  " " << memory_data.rkey() << '\n';

  int POLLING_TIMEOUT_MS = 100;
  while(true) {
    bool result = _state.nonblocking_poll_events(POLLING_TIMEOUT_MS);
    std::cerr << "Polled" << std::endl;
    auto [conn, conn_status] = _state.poll_events(
      false
    );
    std::cerr << "Polled 2 " << static_cast<int>(conn_status) << std::endl;
    if(conn == nullptr){
      std::cerr << "Failed connection creation" << std::endl;
      continue;
    }
    if(conn_status == rdmalib::ConnectionStatus::DISCONNECTED) {
      // FIXME: handle disconnect
      std::cerr << "[Manager-listen] Disconnection on connection {}" << '\n';
      std::cerr << static_cast<int>(memory_data.data()[0]) << '\n';
      break;
    }
    // When client connects, we need to fill the receive queue with work requests before
    // accepting connection. Otherwise, we could accept before we're ready to receive data.
    else if(conn_status == rdmalib::ConnectionStatus::REQUESTED) {
      std::cerr << "Polled, accept!" << std::endl;
      _state.accept(conn);
      conn->post_send(memory_cfg, 0, 0);
      conn->poll_wc(rdmalib::QueueType::SEND, true, 1);
      //sleep(5);
      //break;
    }
  }

  return size;
}

