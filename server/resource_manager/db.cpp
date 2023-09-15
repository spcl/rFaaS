
#include <fstream>

#include <spdlog/spdlog.h>
#include <tuple>
#include <utility>

#include "db.hpp"
#include "rfaas/resources.hpp"

namespace rfaas { namespace resource_manager {

  ExecutorDB::ResultCode ExecutorDB::add(const std::string & node_name, const std::string & ip_address, int port, int cores)
  {
    if(node_name.length() < rfaas::server_data::NODE_NAME_LENGTH) {
      return ResultCode::MALFORMED_DATA;
    }

    // Obtain write access
    writer_lock_t lock(_mutex);

    auto [it, success] = _nodes.try_emplace(node_name, std::make_shared<rfaas::server_data>(node_name, ip_address, port, cores));

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
    servers.deserialize(in_db);

    for(const auto & instance : servers._data) {

      auto [it, success] = _nodes.try_emplace(instance.node, std::make_shared<rfaas::server_data>(instance.node, instance.address, instance.port, instance.cores));

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
      servers._data.emplace_back(instance->node, instance->address, instance->port, instance->cores);
    }

    std::ofstream out{path};
    servers.write(out);
  }

}}

