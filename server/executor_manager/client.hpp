
#ifndef __SERVER_EXECUTOR_MANAGER_CLIENT_HPP__
#define __SERVER_EXECUTOR_MANAGER_CLIENT_HPP__

#include <atomic>
#include <cstdint>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/buffer.hpp>

#include "accounting.hpp"
#include "executor_process.hpp"

namespace rfaas {
  struct AllocationRequest;
}

namespace rfaas::executor_manager {

  struct ResourceManagerConnection;

  struct Client
  {
    static constexpr int RECV_BUF_SIZE = 8;
    rdmalib::Connection* connection;
    rdmalib::Buffer<rfaas::AllocationRequest> allocation_requests;
    std::unique_ptr<ActiveExecutor> executor;
    rdmalib::Buffer<Accounting> accounting;
    uint32_t allocation_time;
    bool _active;
    bool _keep_warm;
    int _id;
    int _client_id;
    int _lease_id;

    Client(int id, rdmalib::Connection* conn, ibv_pd* pd, bool active, bool keep_warm);
    Client(Client &&);
    Client& operator=(Client &&);
    ~Client();
    void reload_queue();
    void disable(ResourceManagerConnection* res_mgr_connection);
    bool active();

    bool has_warm_container();

    int id() const
    {
      return _id;
    }
  };

}

#endif

