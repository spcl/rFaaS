
#include <fstream>

#include <optional>
#include <spdlog/spdlog.h>
#include <tuple>
#include <utility>

#include <rfaas/allocation.hpp>
#include <rfaas/resources.hpp>

#include "db.hpp"

namespace rfaas { namespace resource_manager {

  ExecutorDB::ResultCode ExecutorDB::add(const std::string & node_name, const std::string & ip_address, int port, int cores, int memory)
  {
    if(node_name.length() < rfaas::server_data::NODE_NAME_LENGTH) {
      return ResultCode::MALFORMED_DATA;
    }

    // Obtain write access
    writer_lock_t lock(_mutex);

    auto [it, success] = _nodes.try_emplace(node_name, std::make_shared<node_data>(node_name, ip_address, port, cores, memory));

    if(!success) {
      return ResultCode::EXECUTOR_EXISTS;
    }

    _free_nodes.push_back(it->second);

    spdlog::debug("Adding new executor {} with {}:{} address and {} cores", node_name, ip_address, port, cores);
    return ResultCode::OK;
  }

  ExecutorDB::ResultCode ExecutorDB::remove(const std::string & node_name)
  {
    // Obtain write access
    writer_lock_t lock(_mutex);
    size_t erased = _nodes.erase(node_name);
    return erased > 0 ? ResultCode::OK : ResultCode::EXECUTOR_DOESNT_EXIST;
  }

  bool ExecutorDB::open_lease(int numcores, int memory, rfaas::LeaseResponse& lease)
  {
    // Obtain write access
    writer_lock_t lock(_mutex);

    if(!_free_nodes.size()) {
      return false;
    }

    std::weak_ptr<node_data> used_node;

    auto it = _free_nodes.begin();
    while(it != _free_nodes.end()) {

      auto ptr = *it;

      // Verify the node has not been removed
      if(ptr.expired()) {
        continue;
      }

      auto shared_ptr = ptr.lock();

      // Not enough memory? skip
      if(shared_ptr->free_memory < memory) {
        continue;
      }

      if(shared_ptr->free_cores < numcores) {
        continue;
      }

      lease.lease_id = _lease_count++;
      lease.port = shared_ptr->port;
      strncpy(lease.address, shared_ptr->address, node_data::ADDRESS_LENGTH);

      bool is_total;
      if(shared_ptr->free_cores > numcores && shared_ptr->free_memory > memory) {

        shared_ptr->free_cores -= numcores;
        shared_ptr->free_memory -= memory;
        is_total = true;

      } else {

        _free_nodes.erase(it);
        is_total = false;

      }

      _leases.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(lease.lease_id),
        std::forward_as_tuple(numcores, memory, is_total, std::move(ptr))
      );

      return true;

    }

    return false;
  }

  void ExecutorDB::close_lease(uint32_t lease_id)
  {
    auto it = _leases.find(lease_id);
    if(it == _leases.end()) {
      spdlog::warn("Ignoring non-existing lease {}", lease_id);
      return;
    }

    auto ptr = (*it).second.node;
    auto shared_ptr = ptr.lock();
    if(!shared_ptr) {
      return;
    }

    if((*it).second.total) {

      _free_nodes.push_back(ptr);

    } else {

      shared_ptr->free_cores += (*it).second.cores;
      shared_ptr->free_memory += (*it).second.memory;

    }

  }

  ExecutorDB::reader_lock_t ExecutorDB::read_lock()
  {
    return reader_lock_t(_mutex);
  }

  void ExecutorDB::read(const std::string & path)
  {
    writer_lock_t lock{_mutex};
    std::ifstream in_db{path};
    if(!in_db.is_open()) {
      spdlog::error("Couldn't open the file {}!", path);
    }

    rfaas::servers servers;
    servers.read(in_db);

    for(const auto & instance : servers._data) {

      auto [it, success] = _nodes.try_emplace(instance.node,
        std::make_shared<node_data>(instance.node, instance.address, instance.port, instance.cores, instance.memory)
      );

      if(success) {
        _free_nodes.push_back(it->second);
      } else {
        spdlog::debug("Ignoring duplicate node: {}", instance.node);
      }

    }
  }

  void ExecutorDB::write(const std::string & path)
  {
    reader_lock_t lock{_mutex};
    rfaas::servers servers;

    for(const auto & [key, instance] : _nodes) {
      servers._data.emplace_back(instance->node, instance->address, instance->port, instance->cores, instance->memory);
    }

    std::ofstream out{path};
    servers.write(out);
  }

}}

