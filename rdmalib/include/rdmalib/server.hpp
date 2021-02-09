
#include <vector>
#include <cstdint>
#include <string>
#include <fstream>

#include <cereal/cereal.hpp>

#include <rdmalib/buffer.hpp>

namespace rdmalib { namespace server {

  struct ServerStatus {
    struct Buffer {
      uintptr_t addr;
      uint32_t rkey;
      size_t size;
      template<class Archive>
      void serialize(Archive & ar)
      {
        ar(CEREAL_NVP(addr), CEREAL_NVP(rkey), CEREAL_NVP(size));
      }
    };
    std::vector<Buffer> _buffers;
    std::string _address;
    int _port;

    ServerStatus();
    ServerStatus(std::string address, int port);

    template<typename T>
    void add_buffer(const rdmalib::Buffer<T> & mr)
    {
      _buffers.push_back({mr.ptr(), mr.rkey(), mr.size()});  
    }

    static ServerStatus deserialize(std::istream & in);
    void serialize(std::ostream & out) const;
  };

}}
