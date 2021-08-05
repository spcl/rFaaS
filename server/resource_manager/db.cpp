
#include <fstream>

#include <spdlog/spdlog.h>

#include "db.hpp"

namespace rfaas { namespace resource_manager {

  ExecutorDB::ResultCode ExecutorDB::add(const std::string & ip_address, int port, int cores)
  {
    // Obtain write access
    writer_lock_t lock(_mutex);
    _data._data.emplace_back(ip_address, port, cores);
    //std::sort(_data._data.begin(), _data._data.end(),
    //    []
    spdlog::debug("Adding new executor with {}:{} address and {} cores", ip_address, port, cores);
    return ResultCode::OK;
  }

  ExecutorDB::ResultCode ExecutorDB::remove(const std::string & ip_addresss)
  {
    // Obtain write access
    writer_lock_t lock(_mutex);
    return ResultCode::OK;
  }

  ExecutorDB::reader_lock_t ExecutorDB::read_lock()
  {
    return reader_lock_t(_mutex);
  }

  void ExecutorDB::read(const std::string & path)
  {
    writer_lock_t lock{_mutex};
    std::ifstream in_db{path};
    _data.read(in_db);
  }

  void ExecutorDB::write(const std::string & path)
  {
    reader_lock_t lock{_mutex};
    std::ofstream out{path};
    _data.write(out);
  }

}}

