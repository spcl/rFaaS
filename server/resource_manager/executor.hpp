
#ifndef __SERVER_RESOURCE_MANAGER_EXECUTOR_HPP__
#define __SERVER_RESOURCE_MANAGER_EXECUTOR_HPP__

#include <atomic>
#include <cstdint>
#include <chrono>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/buffer.hpp>

#include <rfaas/allocation.hpp>
#include <rfaas/resources.hpp>
#include <unordered_map>

#include "../common/messages.hpp"

namespace rfaas { namespace resource_manager {

  struct Executor;

  struct Lease
  {
    int cores;
    int memory;
    bool total;
    std::weak_ptr<Executor> node;

    Lease(int cores, int memory, bool total, std::weak_ptr<Executor> && node):
      cores(cores),
      memory(memory),
      total(total),
      node(node)
    {}
  };

  struct Executor : server_data
  {
    rdmalib::Connection* _connection;
    int _free_cores;
    int _free_memory;

    static constexpr int RECV_BUF_SIZE = 32;
    static constexpr int MSG_SIZE = std::max(sizeof(common::NodeRegistration), sizeof(common::LeaseDeallocation));
    rdmalib::Buffer<uint8_t> _receive_buffer;
    rdmalib::Buffer<common::LeaseAllocation> _send_buffer;

    Executor();
    Executor(const std::string & node_name, const std::string & ip, int32_t port, int16_t cores, int32_t memory);

    void initialize_data(const std::string & node_name, const std::string & ip, int32_t port, int16_t cores, int32_t memory);
    void initialize_connection(ibv_pd* pd, rdmalib::Connection* conn);
    bool is_initialized() const;

    bool lease(int cores, int memory);
    bool is_fully_leased() const;
    void cancel_lease(const Lease & lease);

    void merge(std::shared_ptr<Executor>& exec);

    void polled_wc()
    {
      _connection->receive_wcs().update_requests(-1);
    }
  };

  struct Executors
  {
    std::unordered_map<std::string, std::shared_ptr<Executor>> _executors_by_name;
    std::unordered_map<uint32_t, std::shared_ptr<Executor>> _executors_by_conn;
    typedef std::unordered_map<std::string, std::shared_ptr<Executor>>::iterator iter_t;

    ibv_pd* _pd;

    Executors(ibv_pd* pd);

    std::tuple<std::weak_ptr<Executor>, bool> add_executor(const std::string& name, const std::string & ip, int32_t port, int16_t cores, int32_t memory);
    void connect_executor(std::shared_ptr<Executor> && exec);
    bool register_executor(uint32_t qp_num, const std::string& name);

    bool remove_executor(const std::string& name);
    bool remove_executor(uint32_t qp_num);
    std::shared_ptr<Executor> get_executor(const std::string& name);
    std::shared_ptr<Executor> get_executor(uint32_t qp_num);

    void _initialize_connection(rdmalib::Connection* conn);

    iter_t begin();
    iter_t end();
  };

}}

#endif
