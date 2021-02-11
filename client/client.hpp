
#include <vector>
#include <fstream>

#include <cxxopts.hpp>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/server.hpp>

namespace client {

  cxxopts::ParseResult options(int argc, char ** argv);

  struct ServerConnection {
    std::vector<rdmalib::Buffer<char>> _send, _rcv;
    rdmalib::Buffer<char> _submit_buffer;
    rdmalib::Buffer<uint64_t> _atomic_buffer;
    rdmalib::server::ServerStatus _status;
    rdmalib::RDMAActive _active;

    ServerConnection(const rdmalib::server::ServerStatus &);

    bool connect();
    void allocate_send_buffers(int count, uint32_t size);
    void allocate_receive_buffers(int count, uint32_t size);
    rdmalib::Buffer<char> & send_buffer(int idx);
    rdmalib::Buffer<char> & recv_buffer(int idx);

    int submit(int numcores, std::string fname);
    void poll_completion(int);
  };


}
