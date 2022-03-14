

#ifndef __RFAAS_RESOURCE_MANAGER_SETTINGS_HPP__
#define __RFAAS_RESOURCE_MANAGER_SETTINGS_HPP__

#include <string>

#include <rfaas/devices.hpp>

#include <cereal/details/helpers.hpp>

namespace rfaas::resource_manager {

  // Manager configuration settings.
  // Includes the RDMA connection, and the HTTP connection.
  struct Settings
  {
    std::string rdma_device;
    int rdma_device_port;
    rfaas::device_data* device;
    
    std::string http_network_address;
    uint16_t http_network_port;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(rdma_device), CEREAL_NVP(rdma_device_port),
        CEREAL_NVP(http_network_address), CEREAL_NVP(http_network_port)
      );
    }

    static Settings deserialize(std::istream & in);
  };

}

#endif

