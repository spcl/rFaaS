
#include <fstream>

#include <optional>
#include <spdlog/spdlog.h>
#include <tuple>
#include <utility>

#include <rfaas/allocation.hpp>
#include <rfaas/resources.hpp>

#include "common/messages.hpp"
#include "db.hpp"

namespace rfaas { namespace resource_manager {

  ExecutorDB::ResultCode ExecutorDB::add(const std::string & node_name, const std::string & ip_address, int port, int cores, int memory)
  {
    if(node_name.length() > rfaas::server_data::NODE_NAME_LENGTH) {
      return ResultCode::MALFORMED_DATA;
    }

    // Obtain write access
    writer_lock_t lock(_mutex);

    auto [ptr, success] = _executors.add_executor(node_name, ip_address, port, cores, memory);

    if(!success) {
      return ResultCode::EXECUTOR_EXISTS;
    }

    _free_nodes.push_back(ptr);

    spdlog::debug("Adding new executor {} with {}:{} address and {} cores", node_name, ip_address, port, cores);
    return ResultCode::OK;
  }

  ExecutorDB::ResultCode ExecutorDB::remove(const std::string & node_name)
  {
    // Obtain write access
    writer_lock_t lock(_mutex);
    bool erased = _executors.remove_executor(node_name);
    return erased ? ResultCode::OK : ResultCode::EXECUTOR_DOESNT_EXIST;
  }

  std::shared_ptr<Executor> ExecutorDB::open_lease(int numcores, int memory, rfaas::LeaseResponse& lease)
  {
    // Obtain write access
    writer_lock_t lock(_mutex);

    if(!_free_nodes.size()) {
      SPDLOG_DEBUG("No available executors!");
      return nullptr;
    }

    std::weak_ptr<Executor> used_node;

    auto it = _free_nodes.begin();
    while(it != _free_nodes.end()) {

      std::weak_ptr<Executor>& ptr = *it;
      auto shared_ptr = ptr.lock();

      // Verify the node has not been removed
      if(!shared_ptr) {
        _free_nodes.erase(it++);
        SPDLOG_DEBUG("Node cannot be used, no longer valid!");
        continue;
      }

      // Can't use an executor that has not been used yet
      if(!shared_ptr->is_initialized()) {
        ++it;
        SPDLOG_DEBUG("Node {} cannot be used, not initialized!", shared_ptr->node);
        continue;
      }

      if(!shared_ptr->lease(numcores, memory)) {
        ++it;
        SPDLOG_DEBUG("Node {} cannot be used, not enough resources!", shared_ptr->node);
        continue;
      }

      lease.lease_id = _lease_count++;
      lease.port = shared_ptr->port;
      strncpy(lease.address, shared_ptr->address.c_str(), Executor::ADDRESS_LENGTH);

      bool is_total = shared_ptr->is_fully_leased();
      if(is_total) {
        _free_nodes.erase(it);
      }

      _leases.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(lease.lease_id),
        std::forward_as_tuple(numcores, memory, is_total, std::move(ptr))
      );

      return shared_ptr;

    }

    return nullptr;
  }

  void ExecutorDB::close_lease(common::LeaseDeallocation & msg)
  {
    writer_lock_t lock(_mutex);

    auto it = _leases.find(msg.lease_id);
    if(it == _leases.end()) {
      spdlog::warn("Ignoring non-existing lease {}", msg.lease_id);
      return;
    }

    auto ptr = (*it).second.node;
    auto shared_ptr = ptr.lock();
    if(!shared_ptr) {
      return;
    }

    SPDLOG_DEBUG("Cancelled lease {}, allocation took {} us.", msg.lease_id, msg.allocation_time);

    shared_ptr->cancel_lease((*it).second);

    if((*it).second.total) {
      _free_nodes.push_back(ptr);
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

      auto [weak_ptr, success] = _executors.add_executor(instance.node, instance.address, instance.port, instance.cores, instance.memory);

      if(success) {
        _free_nodes.push_back(weak_ptr);
      } else {
        spdlog::debug("Ignoring duplicate node: {}", instance.node);
      }

    }
  }

  void ExecutorDB::write(const std::string & path)
  {
    reader_lock_t lock{_mutex};
    rfaas::servers servers;

    for(const auto & [key, instance] : _executors) {
      servers._data.emplace_back(instance->node, instance->address, instance->port, instance->cores, instance->memory);
    }

    std::ofstream out{path};
    servers.write(out);
  }

}}

