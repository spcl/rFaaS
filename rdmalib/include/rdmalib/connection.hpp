
#ifndef __RDMALIB_CONNECTION_HPP__
#define __RDMALIB_CONNECTION_HPP__

#include <cstdint>
#include <vector>
#include <optional>

#include <infiniband/verbs.h>

#include <rdma/rdma_cma.h>
#include <rdmalib/buffer.hpp>

namespace rdmalib {

  enum class QueueType{
    SEND,
    RECV
  };

  struct ConnectionConfiguration {
    // Configuration of QP
    ibv_qp_init_attr attr;
    rdma_conn_param conn_param;

    ConnectionConfiguration();
  };

  struct ScatterGatherElement {
    // smallvector in practice
    std::vector<ibv_sge> _sges;

    ScatterGatherElement();

    template<typename T>
    ScatterGatherElement(const Buffer<T> & buf)
    {
      add(buf);
    }

    template<typename T>
    void add(const Buffer<T> & buf)
    {
      //emplace_back for structs will be supported in C++20
      _sges.push_back({buf.address(), buf.bytes(), buf.lkey()});
    }

    ibv_sge * array();
    size_t size();
  };

  // State of a communication:
  // a) communication ID
  // b) Queue Pair
  struct Connection {
    rdma_cm_id* _id;
    ibv_qp* _qp; 
    int32_t _req_count;
    static const int _wc_size = 32; 
    std::array<ibv_wc, _wc_size> _swc; // fast fix for overlapping polling
    std::array<ibv_wc, _wc_size> _rwc;
    int _send_flags;

    static const int _rbatch = 32; // 32 for faster division in the code
    struct ibv_recv_wr _batch_wrs[_rbatch]; // preallocated and prefilled batched recv.
 
    Connection();
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&);

    void inlining(bool enable);
    void close();
    ibv_qp* qp() const;
    // Blocking, no timeout
    std::tuple<ibv_wc*, int> poll_wc(QueueType, bool blocking = true);
    int32_t post_send(ScatterGatherElement && elem, int32_t id = -1);
    int32_t post_recv(ScatterGatherElement && elem, int32_t id = -1, int32_t count = 1);

    int32_t post_batched_empty_recv(int32_t count = 1);

    int32_t post_write(ScatterGatherElement && elems, const RemoteBuffer & buf);
    int32_t post_write(ScatterGatherElement && elems, const RemoteBuffer & buf, uint32_t immediate);
    int32_t post_cas(ScatterGatherElement && elems, const RemoteBuffer & buf, uint64_t compare, uint64_t swap);
  private:
    int32_t _post_write(ScatterGatherElement && elems, ibv_send_wr wr);
  };
}

#endif

