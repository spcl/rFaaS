
#include <cereal/archives/json.hpp>

#include <rdmalib/server.hpp>

namespace rdmalib { namespace server {

  ServerStatus::ServerStatus():
    _address(""),
    _port(0)
  {}

  ServerStatus::ServerStatus(std::string address, int port):
    _address(address),
    _port(port)
  {}

  ServerStatus ServerStatus::deserialize(std::istream & in)
  {
    ServerStatus status;
    cereal::JSONInputArchive archive_in(in);
    archive_in(status);
    return status;
  }

  void ServerStatus::serialize(std::ostream & out) const
  {
    cereal::JSONOutputArchive archive_out(out);
    archive_out(*this);
  }

}}
