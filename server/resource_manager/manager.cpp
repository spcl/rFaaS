
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "manager.hpp"

namespace rfaas::resource_manager {

  Manager::Manager(Settings & settings):
    _executors_output_path(),
    _state(settings.device->ip_address, settings.rdma_device_port,
        settings.device->default_receive_buffer_size, true,
        settings.device->max_inline_data),
    _http_server(_executor_data, settings)
  {


  }

  void Manager::start()
  {
    _http_server.start();
  }

  void Manager::shutdown()
  {
    _http_server.stop();
  }

  void Manager::read_database(const std::string & path)
  {
    _executor_data.read(path);
  }

  void Manager::set_database_path(const std::string & name)
  {
    _executors_output_path = name;
  }

  void Manager::dump_database()
  {
    if(_executors_output_path.has_value()) {
      spdlog::debug("Writing resource manager database to {}", _executors_output_path.value());
      _executor_data.write(_executors_output_path.value());
    }
  }

}
