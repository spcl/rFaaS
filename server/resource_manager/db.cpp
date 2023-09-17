
#include <fstream>

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

  int ExecutorDB::lease(int numcores, int memory, rfaas::LeaseResponse & results)
  {
    // Obtain write access
    writer_lock_t lock(_mutex);

    if(!_free_nodes.size()) {
      return 0;
    }

    int cores_sum = 0;
    std::vector<std::weak_ptr<node_data>> used_nodes;
    int result_count = 0;

    while(cores_sum < numcores && !_free_nodes.empty()) {

      auto ptr = _free_nodes.front();

      // Verify the node has not been removed
      if(ptr.expired()) {
        _free_nodes.pop_front();
        continue;
      }

      auto shared_ptr = ptr.lock();

      // Not enough memory? skip
      if(shared_ptr->free_memory < memory * shared_ptr->free_cores) {
        continue;
      }

      // Are we going to use all cores?
      int cores_missing = numcores - cores_sum;
      int cores_available = shared_ptr->free_cores;
      if(cores_missing >= cores_available) {

        // Block the node
        results.nodes[result_count].cores = cores_available;
        results.nodes[result_count].port = shared_ptr->port;
        strncpy(results.nodes[result_count].address, shared_ptr->address, node_data::ADDRESS_LENGTH);

        result_count++;

        // We are done
        if (cores_missing == cores_available) {
          return result_count;
        } else {
          used_nodes.push_back(ptr);
        }

      } else {

        // We found enough cores, but part of the node remains available.
        results.nodes[result_count].cores = cores_missing;
        results.nodes[result_count].port = shared_ptr->port;
        strncpy(results.nodes[result_count].address, shared_ptr->address, node_data::ADDRESS_LENGTH);

        _free_nodes.push_front(ptr);
        shared_ptr->free_cores = cores_missing;
        shared_ptr->free_memory -= memory * cores_missing;

        return result_count;
      }

      if(result_count == LeaseResponse::MAX_NODES_PER_LEASE) {
        break;
      }

    }

    // We have not found enough
    for(auto & node : used_nodes) {
      _free_nodes.push_back(node);
    }
    return 0;
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

