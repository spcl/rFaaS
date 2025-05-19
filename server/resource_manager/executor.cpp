
#include "executor.hpp"
#include "rdmalib/connection.hpp"
#include <utility>

namespace rfaas::resource_manager {

  Executor::Executor():
    _connection(nullptr),
    _free_cores(0),
    _free_memory(0),
    _receive_buffer(RECV_BUF_SIZE * MSG_SIZE),
    _send_buffer(1)
  {}

  Executor::Executor(const std::string & node_name, const std::string & ip, int32_t port, int16_t cores, int32_t memory):
    _connection(nullptr),
    _free_cores(0),
    _free_memory(0),
    _receive_buffer(RECV_BUF_SIZE * MSG_SIZE),
    _send_buffer(1)
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

  //void Executor::initialize_connection(rdmalib::Connection* conn)
  //{
  //  _connection = conn;

  //  // Make the buffer accessible to clients
  //}

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
    _pd(pd)
  {
  }

  void Executor::initialize_connection(ibv_pd* pd, rdmalib::Connection* conn)
  {
    this->_connection = conn;

    _receive_buffer.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    _send_buffer.register_memory(pd, IBV_ACCESS_LOCAL_WRITE);

    this->_connection->receive_wcs().initialize(_receive_buffer, MSG_SIZE);
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

  void Executors::connect_executor(std::shared_ptr<Executor> && exec)
  {
    uint32_t qp_num = exec->_connection->qp()->qp_num;
    _executors_by_conn[qp_num] = std::move(exec);
  }

  bool Executors::register_executor(uint32_t qp_num, const std::string& name)
  {

    // Already registered
    auto conn_it = _executors_by_conn.find(qp_num);
    if(conn_it == _executors_by_conn.end()) {
      return false;
    }

    // Two situations
    // 
    // (1) The executor has already connected but doesn't have a name yet.
    // Thus, we make a copy under the correct name.
    //
    // (2) The executor has a name and it is connected - we need now
    // to connect both of them.
    // We cannot overwrite the `name` value, as the executor DB is now
    // holding a weak ptr to it.
    // Instead, we overwrite the `conn` value.
    //
    auto exec_it = _executors_by_name.find(name);
    if(exec_it == _executors_by_name.end()) {
      _executors_by_name[name] = ((*conn_it).second);
    } else {
      (*exec_it).second->merge((*conn_it).second);
      _executors_by_conn[qp_num] = (*exec_it).second;
    }
    SPDLOG_DEBUG("Registered executor with name {}, qp num {}", name, qp_num);

    return true;
  }

  void Executor::merge(std::shared_ptr<Executor>& exec)
  {
    SPDLOG_DEBUG("Merge two executors {}", fmt::ptr(exec->_connection));
    this->_connection = exec->_connection;

    this->_receive_buffer = std::move(exec->_receive_buffer);
    this->_send_buffer = std::move(exec->_send_buffer);
    
  }

  bool Executors::remove_executor(const std::string& name)
  {
    throw std::runtime_error("Not implemented!");
  }

  bool Executors::remove_executor(uint32_t qp_num)
  {
    auto conn_it = _executors_by_conn.find(qp_num);
    if(conn_it != _executors_by_conn.end()) {

      auto it = _executors_by_name.find((*conn_it).second->node);
      if(it != _executors_by_name.end()) {
        _executors_by_name.erase(it);
      }

      _executors_by_conn.erase(conn_it);
      return true;
    }
    return false;
  }

  std::shared_ptr<Executor> Executors::get_executor(const std::string& name)
  {
    auto conn_it = _executors_by_name.find(name);
    if(conn_it != _executors_by_name.end()) {
      return (*conn_it).second;
    }
    return nullptr;
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

