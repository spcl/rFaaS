
#include <chrono>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <rdmalib/allocation.hpp>

#include "client.hpp"

namespace rfaas::executor_manager {

  Client::Client(rdmalib::Connection* conn, ibv_pd* pd): //, Accounting & _acc):
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
    accounting.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
    // Make the buffer accessible to clients
    allocation_requests.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
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
    rdma_disconnect(connection->id());
    SPDLOG_DEBUG(
      "[Client] Disconnect client with connection {} id {}",
      fmt::ptr(connection), fmt::ptr(connection->id())
    );
    // First, we check if the child is still alive
    if(executor) {
      int status;
      auto b = std::chrono::high_resolution_clock::now();
      kill(executor->id(), SIGKILL);
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
    connection->close();
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

