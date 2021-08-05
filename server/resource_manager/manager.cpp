
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "manager.hpp"

namespace rfaas::resource_manager {

  Settings Settings::deserialize(std::istream & in)
  {
    Settings settings{};
    cereal::JSONInputArchive archive_in(in);
    archive_in(cereal::make_nvp("config", settings));

    // read RDMA device details
    rfaas::device_data * dev = rfaas::devices::instance().device(settings.rdma_device);
    if(!dev) {
      spdlog::error("Data for device {} not found!", settings.rdma_device);
      throw std::runtime_error{"Unknown device!"};
    }
    settings.device = dev;
    return settings;
  }

  Manager::Manager(Settings & settings):
    _state(settings.device->ip_address, settings.rdma_device_port,
        settings.device->default_receive_buffer_size, true,
        settings.device->max_inline_data),
    _executors_output_path()
  {


  }

  void Manager::start()
  {

  }

  void Manager::read_database(const std::string & name)
  {
    std::ifstream in_db{name};
    _executors_data.read(in_db);
  }

  void Manager::set_database_path(const std::string & name)
  {
    _executors_output_path = name;
  }

  void Manager::dump_database()
  {
    if(_executors_output_path.has_value()) {
      std::ofstream out{_executors_output_path.value()};
      spdlog::debug("Writing resource manager database to {}", _executors_output_path.value());
      _executors_data.write(out);
    }
  }
}
