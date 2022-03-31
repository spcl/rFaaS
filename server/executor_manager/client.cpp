
#include <chrono>

#include <fcntl.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <rdmalib/allocation.hpp>

#include "client.hpp"

namespace rfaas::executor_manager {

  #ifdef USE_LIBFABRIC
  Client::Client(rdmalib::Connection* conn, fid_domain* pd):
  #else
  Client::Client(rdmalib::Connection* conn, ibv_pd* pd):
  #endif
    connection(conn),
    allocation_requests(RECV_BUF_SIZE),
    rcv_buffer(RECV_BUF_SIZE),
    accounting(1),
    //accounting(_acc),
    allocation_time(0),
    _active(false)
  {
    // Make the buffer accessible to clients
    memset(accounting.data(), 0, accounting.data_size());
    #ifdef USE_LIBFABRIC
    accounting.register_memory(pd, FI_READ | FI_WRITE | FI_REMOTE_WRITE);
    #else
    accounting.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
    #endif
    // Make the buffer accessible to clients
    #ifdef USE_LIBFABRIC
    allocation_requests.register_memory(pd, FI_READ | FI_WRITE | FI_REMOTE_WRITE);
    #else
    allocation_requests.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    #endif
    // Initialize batch receive WCs
    connection->initialize_batched_recv(allocation_requests, sizeof(rdmalib::AllocationRequest));
    rcv_buffer.connect(connection);
  }

  //void Client::reinitialize(rdmalib::Connection* conn)
  //{
  //  connection = conn;
  //  // Initialize batch receive WCs
  //  connection->initialize_batched_recv(allocation_requests, sizeof(rdmalib::AllocationRequest));
  //  rcv_buffer.connect(conn);
  //}

  void Client::reload_queue()
  {
    rcv_buffer.refill();
  }

  void Client::disable(int id)
  {
    #ifndef USE_LIBFABRIC
    rdma_disconnect(connection->id());
    #endif
    SPDLOG_DEBUG(
      "[Client] Disconnect client with connection {} id {}",
      fmt::ptr(connection), fmt::ptr(connection->id())
    );
    // First, we check if the child is still alive
    if(executor) {
      int status;
      auto b = std::chrono::high_resolution_clock::now();
      kill(executor->id(), SIGKILL); // for executor need a SIGINT
      waitpid(executor->id(), &status, WUNTRACED);
      auto e = std::chrono::high_resolution_clock::now();
      spdlog::info("Waited for child {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(e-b).count());

      executor.reset();
    }
    spdlog::info(
      "Client {} exited, time allocated {} us, polling {} us, execution {} us",
      id, allocation_time,
      accounting.data()[0].hot_polling_time,
      accounting.data()[0].execution_time
    );
    //acc.hot_polling_time = acc.execution_time = 0;
    // SEGFAULT?
    //ibv_dereg_mr(allocation_requests._mr);
    #ifndef USE_LIBFABRIC
    connection->close();
    #endif
    delete connection;
    connection = nullptr;
    _active=false;
  }

  bool Client::active()
  {
    // Compiler complains for some reason
    return _active;
  }

}

