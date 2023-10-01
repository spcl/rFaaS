
#include <chrono>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <rfaas/allocation.hpp>
#include <rfaas/connection.hpp>

#include "client.hpp"
#include "manager.hpp"

namespace rfaas::executor_manager {

  Client::Client(int id, rdmalib::Connection* conn, ibv_pd* pd, bool active): //, Accounting & _acc):
    connection(conn),
    allocation_requests(RECV_BUF_SIZE),
    accounting(1),
    //accounting(_acc),
    allocation_time(0),
    _active(active),
    _id(id)
  {
    // Make the buffer accessible to clients
    memset(accounting.data(), 0, accounting.data_size());
    accounting.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
    // Make the buffer accessible to clients
    allocation_requests.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

    // Initialize batch receive WCs
    connection->receive_wcs().initialize(allocation_requests);
  }

  Client::Client(Client && obj):
    connection(obj.connection),
    allocation_requests(std::move(obj.allocation_requests)),
    executor(std::move(obj.executor)),
    accounting(std::move(obj.accounting)),
    allocation_time(std::move(obj.allocation_time)),
    _active(std::move(obj._active))
  {
    obj.connection = nullptr;
  }

  Client& Client::operator=(Client && obj)
  {
    connection = obj.connection;
    allocation_requests = std::move(obj.allocation_requests);
    executor = std::move(obj.executor);
    accounting = std::move(obj.accounting);
    allocation_time = std::move(obj.allocation_time);
    _active = std::move(obj._active);

    obj.connection = nullptr;

    return *this;
  }

  Client::~Client()
  {
    if(connection) {
      connection->close();
      delete connection;
    }
  }

  void Client::reload_queue()
  {
    connection->receive_wcs().refill();
  }

  void Client::disable(ResourceManagerConnection* res_mgr_connection)
  {

    if(executor) {
      auto now = std::chrono::high_resolution_clock::now();
      allocation_time +=
        std::chrono::duration_cast<std::chrono::microseconds>(
          now - executor->_allocation_finished
        ).count();
    }

    rdma_disconnect(connection->id());
    SPDLOG_DEBUG(
      "[Client] Disconnect client with connection {} id {}",
      fmt::ptr(connection), fmt::ptr(connection->id())
    );
    // First, we check if the child is still alive
    if(executor) {
      int status;
      auto b = std::chrono::high_resolution_clock::now();
      kill(executor->id(), SIGTERM);
      waitpid(executor->id(), &status, WUNTRACED);
      auto e = std::chrono::high_resolution_clock::now();
      spdlog::info("Waited for child {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(e-b).count());
      executor.reset();
    }
    spdlog::info(
      "Client {} exited, time allocated {} us, polling {} us, execution {} us",
      _id, allocation_time,
      accounting.data()[0].hot_polling_time / 1000.0,
      accounting.data()[0].execution_time / 1000.0
    );

    if(res_mgr_connection) {

      res_mgr_connection->close_lease(
        _id,
        allocation_time,
        accounting.data()[0].execution_time,
        accounting.data()[0].hot_polling_time
      );

    }

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

