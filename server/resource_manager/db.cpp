
#include <fstream>

#include "db.hpp"

namespace rfaas { namespace resource_manager {

  ExecutorDB::ResultCode ExecutorDB::add(const std::string & request)
  {
    // Obtain write access
    std::unique_lock lock(_mutex);
    //_data._data.emplace_back({});
    std::cout << request << std::endl;
    return ResultCode::OK;
  }

  ExecutorDB::ResultCode ExecutorDB::remove(const std::string & request)
  {
    // Obtain write access
    std::unique_lock lock(_mutex);
    //_data._data.emplace_back({});
    std::cout << request << std::endl;

    return ResultCode::OK;
  }

  ExecutorDB::lock_t ExecutorDB::read_lock()
  {
    return std::shared_lock(_mutex);
  }

  void ExecutorDB::read(const std::string & path)
  {
    std::ifstream in_db{path};
    _data.read(in_db);
  }

  void ExecutorDB::write(const std::string & path)
  {
    std::ofstream out{path};
    _data.write(out);
  }

}}

