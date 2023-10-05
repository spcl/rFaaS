
#include <chrono>

#include <fcntl.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <rfaas/allocation.hpp>
#include <rfaas/connection.hpp>

#include "client.hpp"
#include "manager.hpp"

namespace rfaas::executor_manager {

  #ifdef USE_LIBFABRIC
  Client::Client(int id, rdmalib::Connection* conn, fid_domain* pd, bool active):
  #else
  Client::Client(int id, rdmalib::Connection* conn, ibv_pd* pd, bool active):
  #endif
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

  std::string exec(const char *cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
      throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
    }
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());
    return result;
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

      // FIXME: this should be enabled only for Sarus
      std::string first_child = exec(fmt::format("pgrep -P {}", executor->id()).c_str());
      std::string second_child = exec(fmt::format("pgrep -P {}", first_child).c_str());

      int pid = std::stoi(second_child);
      // int pid = executor->id();

      int ret = kill(pid, SIGTERM);
      auto b = std::chrono::high_resolution_clock::now();
      spdlog::info(
        "[Client] Kill container {}, status {}", pid, ret
      );
      ret = waitpid(pid, &status, WUNTRACED);
      auto e = std::chrono::high_resolution_clock::now();
      spdlog::info("Waited for child {} ms, ret {}", std::chrono::duration_cast<std::chrono::milliseconds>(e-b).count(), ret);
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
    SPDLOG_DEBUG("Closed client");

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

