
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
        settings.device->max_inline_data)
  {


  }

  void Manager::start()
  {

  }

}
