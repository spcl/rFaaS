
#include <vector>
#include <fstream>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/recv_buffer.hpp>

namespace client {

  struct InvocationResult {
    int buf_idx;
    int execution_id;
  };

  struct ServerConnection {
    std::vector<rdmalib::Buffer<char>> _send, _rcv;
    rdmalib::Buffer<char> _submit_buffer;
    rdmalib::Buffer<uint64_t> _atomic_buffer;
    rdmalib::server::ServerStatus _status;
    rdmalib::RDMAActive _active;
    rdmalib::RecvBuffer _rcv_buffer;
    int _max_inline_data;
    int _msg_size;

    ServerConnection(const rdmalib::server::ServerStatus &, int rcv_buf, int max_inline_data);

    rdmalib::Connection & connection();
    bool connect();
    void allocate_send_buffers(int count, uint32_t size);
    void allocate_receive_buffers(int count, uint32_t size);
    rdmalib::Buffer<char> & send_buffer(int idx);
    rdmalib::Buffer<char> & recv_buffer(int idx);
    rdmalib::RecvBuffer& recv()
    {
      return _rcv_buffer;
    }

    int submit(int numcores, std::string fname);
    int submit_fast(int numcores, std::string fname);
    void poll_completion(int);
  };


}
