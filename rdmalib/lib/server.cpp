
#include <cereal/archives/json.hpp>

#include <rdmalib/server.hpp>

namespace rdmalib { namespace server {

  template <typename Library>
  ServerStatus<Library>::ServerStatus():
    _address(""),
    _port(0)
  {}

  template <typename Library>
  ServerStatus<Library>::ServerStatus(std::string address, int port):
    _address(address),
    _port(port)
  {}

  template <typename Library>
  ServerStatus<Library> ServerStatus<Library>::deserialize(std::istream & in)
  {
    ServerStatus status;
    cereal::JSONInputArchive archive_in(in);
    archive_in(status);
    return status;
  }

  template <typename Library>
  void ServerStatus<Library>::serialize(std::ostream & out) const
  {
    cereal::JSONOutputArchive archive_out(out);
    archive_out(*this);
  }

}}
