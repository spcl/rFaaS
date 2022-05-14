
#include <spdlog/spdlog.h>

#include "settings.hpp"

namespace rfaas::executor_manager {

  SandboxType sandbox_deserialize(std::string type)
  {
    static std::map<std::string, SandboxType> sandboxes = {
      {"process", SandboxType::PROCESS},
      {"docker", SandboxType::DOCKER},
      {"sarus", SandboxType::SARUS}
    };
    return sandboxes.at(type);
  }

  std::string sandbox_serialize(SandboxType type)
  {
    static std::map<SandboxType, std::string> sandboxes = {
      {SandboxType::PROCESS, "process"},
      {SandboxType::DOCKER, "docker"},
      {SandboxType::SARUS, "sarus"}
    };
    return sandboxes.at(type);
  }

  Settings Settings::deserialize(std::istream & in)
  {
    Settings settings{};
    cereal::JSONInputArchive archive_in(in);
    archive_in(cereal::make_nvp("config", settings));
    archive_in(cereal::make_nvp("executor", settings.exec));

    // read RDMA device details
    rfaas::device_data * dev = rfaas::devices::instance().device(settings.rdma_device);
    if(!dev) {
      spdlog::error("Data for device {} not found!", settings.rdma_device);
      throw std::runtime_error{"Unknown device!"};
    }
    settings.device = dev;

    // executor options
    settings.exec.max_inline_data = dev->max_inline_data;
    settings.exec.recv_buffer_size = dev->default_receive_buffer_size;

    return settings;
  }

}

