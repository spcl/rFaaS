
#ifndef __RFAAS_RFAAS_HPP__
#define __RFAAS_RFAAS_HPP__

#include <rfaas/connection.hpp>
#include <rfaas/devices.hpp>
#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>

namespace rfaas {

  struct rfaas {

    rfaas(std::string address, int port, device_data & dev):
      _resource_mgr(address, port, dev.default_receive_buffer_size, dev.max_inline_data)
    {}

    bool connect()
    {
      return _resource_mgr.connect();
    }

  private:
    manager_connection _resource_mgr;
  };

}

#endif
