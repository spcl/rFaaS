
#include "executor.hpp"
#include "rdmalib/connection.hpp"
#include <utility>

namespace rfaas::resource_manager {

  Executor::Executor():
    _node_name{},
    _connection(nullptr),
    _free_cores(0),
    _free_memory(0)
  {}

  Executor::Executor(const std::string & node_name, const std::string & ip, int32_t port, int16_t cores, int32_t memory):
    _connection(nullptr),
    _free_cores(0),
    _free_memory(0)
  {
    this->initialize_data(node_name, ip, port, cores, memory);
  }

  void Executor::initialize_data(const std::string & node_name, const std::string & ip, int32_t port, int16_t cores, int32_t memory)
  {
    this->node = node_name;
    this->address = ip;
    this->port = port;
    this->cores = cores;
    this->memory = memory;
    this->_free_cores = cores;
    this->_free_memory = memory;
  }

  void Executor::initialize_connection(rdmalib::Connection* conn)
  {
    _connection = conn;
  }

  bool Executor::is_initialized() const
  {
    return !node.empty() && _connection != nullptr;
  }

  bool Executor::lease(int cores, int memory)
  {
    // Not enough memory? skip
    if(_free_memory < memory) {
      return false;
    }

    if(_free_cores < cores) {
      return false;
    }

    _free_cores -= cores;
    _free_memory -= memory;

    return true;
  }

  bool Executor::is_fully_leased() const
  {
    return _free_cores == 0 || _free_memory == 0;
  }

  void Executor::cancel_lease(const Lease & lease)
  {
    _free_cores += lease.cores;
    _free_memory += lease.memory;
  }

  Executors::Executors(ibv_pd* pd):
    _receive_buffer(RECV_BUF_SIZE * MSG_SIZE),
    _send_buffer(1),
    _initialized(false)
  {
    // Make the buffer accessible to clients
    _receive_buffer.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    _send_buffer.register_memory(pd, IBV_ACCESS_LOCAL_WRITE);
  }

  void Executors::_initialize_connection(rdmalib::Connection* conn)
  {
    conn->receive_wcs().initialize_batched_recv(_receive_buffer, MSG_SIZE);
  }

  std::tuple<std::weak_ptr<Executor>, bool> Executors::add_executor(const std::string& name, const std::string & ip, int32_t port, int16_t cores, int32_t memory)
  {
    auto exec = std::make_shared<Executor>(name, ip, port, cores, memory);
    auto [it, success] = _executors_by_name.insert(std::make_pair(name, exec));

    // Two possibilities: executor already exists or has been registered?
    if(!success) {

      if((*it).second->is_initialized()) {
        return std::make_tuple(std::weak_ptr<Executor>{}, false);
      } else {
        (*it).second->initialize_data(name, ip, port, cores, memory);
        return std::make_tuple(exec, true);
      }

    }
    // Only one possibility: executor has not connected & registered yet
    else {
      return std::make_tuple(exec, true);
    }
  }

  void Executors::connect_executor(rdmalib::Connection* conn)
  {
    uint32_t qp_num = conn->qp()->qp_num;
    _unregistered_executors[qp_num] = conn;
    conn->receive_wcs().initialize(_receive_buffer, MSG_SIZE);
  }

  bool Executors::register_executor(uint32_t qp_num, const std::string& name)
  {
    auto it = _unregistered_executors.find(qp_num);
    if(it == _unregistered_executors.end()) {
      return false;
    }

    // Already registered
    auto conn_it = _executors_by_conn.find(qp_num);
    if(conn_it != _executors_by_conn.end()) {
      return false;
    }

    // Two situations
    // (1) Fully new executor, not added by the HTTP interface.
    // (2) Executor has been added before.
    auto exec_it = _executors_by_name.find(name);
    if(exec_it == _executors_by_name.end()) {

      SPDLOG_DEBUG("Registered executor with name {}, qp num {}", name, qp_num);
      auto exec = std::make_shared<Executor>();
      exec->initialize_connection((*it).second);
      _executors_by_name[name] = exec;
      
    } else {
      (*exec_it).second->initialize_connection((*it).second);
    }

    return true;
  }

  bool Executors::remove_executor(const std::string& name)
  {
    throw std::runtime_error("Not implemented!");
  }

  std::weak_ptr<Executor> Executors::get_executor(const std::string& name)
  {

  }

  std::shared_ptr<Executor> Executors::get_executor(uint32_t qp_num)
  {
    auto conn_it = _executors_by_conn.find(qp_num);
    if(conn_it != _executors_by_conn.end()) {
      return (*conn_it).second;
    }
    return nullptr;
  }

  Executors::iter_t Executors::begin()
  {
    return _executors_by_name.begin();
  }

  Executors::iter_t Executors::end()
  {
    return _executors_by_name.end();
  }
}

