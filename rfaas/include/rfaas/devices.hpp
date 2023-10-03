
#ifndef __RFAAS_DEVICES_HPP__
#define __RFAAS_DEVICES_HPP__

#include <map>
#include <cstdint>
#include <memory>
#include <cstring>

#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp> 
#include <cereal/types/string.hpp>

namespace rfaas {

  struct device_data
  {
    std::string name;
    std::string ip_address;
    int port;
    uint16_t max_inline_data;
    int16_t default_receive_buffer_size;

    template <class Archive>
    void save(Archive & ar) const
    {
      ar( CEREAL_NVP(name), CEREAL_NVP(ip_address), CEREAL_NVP(port),
          CEREAL_NVP(max_inline_data), CEREAL_NVP(default_receive_buffer_size));
    }

    template <class Archive>
    void load(Archive & ar )
    {
      ar( CEREAL_NVP(name), CEREAL_NVP(ip_address), CEREAL_NVP(port),
          CEREAL_NVP(max_inline_data), CEREAL_NVP(default_receive_buffer_size));
    }
  };

  struct devices
  {
    static std::unique_ptr<devices> _instance;
    std::vector<device_data> _data; 

    device_data * device (std::string name) noexcept;
    device_data * front () noexcept;
    static devices & instance();
    static void deserialize(std::istream & in);
  private:
    devices() {}
  };

}

#endif

