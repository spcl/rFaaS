
#ifndef __RDMALIB_CONNECTION_HPP__
#define __RDMALIB_CONNECTION_HPP__

#include <cstdint>
#include <initializer_list>
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

  // State of a communication:
  // a) communication ID
  // b) Queue Pair
  struct Connection {
    rdma_cm_id* _id;
    ibv_qp* _qp; 
    ibv_comp_channel* _channel;
    int32_t _req_count;
    static const int _wc_size = 32; 
    // FIXME: associate this with RecvBuffer
    std::array<ibv_wc, _wc_size> _swc; // fast fix for overlapping polling
    std::array<ibv_wc, _wc_size> _rwc;
    std::array<ScatterGatherElement, _wc_size> _rwc_sges;
    int _send_flags;

    static const int _rbatch = 32; // 32 for faster division in the code
    struct ibv_recv_wr _batch_wrs[_rbatch]; // preallocated and prefilled batched recv.
 
    Connection();
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&);

    void initialize_batched_recv(const ScatterGatherElement & sge, size_t offset);
    void inlining(bool enable);
    void initialize();
    void close();
    ibv_qp* qp() const;
    // Blocking, no timeout
    std::tuple<ibv_wc*, int> poll_wc(QueueType, bool blocking = true);
    int32_t post_send(ScatterGatherElement && elem, int32_t id = -1, bool force_inline = false);
    int32_t post_recv(ScatterGatherElement && elem, int32_t id = -1, int32_t count = 1);

    int32_t post_batched_empty_recv(int32_t count = 1);

    int32_t post_write(ScatterGatherElement && elems, const RemoteBuffer & buf, bool force_inline = false);
    int32_t post_write(ScatterGatherElement && elems, const RemoteBuffer & buf, uint32_t immediate, bool force_inline = false);
    int32_t post_cas(ScatterGatherElement && elems, const RemoteBuffer & buf, uint64_t compare, uint64_t swap);

    // Register to be notified about all events, including unsolicited ones
    void notify_events();
    ibv_cq* wait_events();
    void ack_events(ibv_cq* cq, int len);
  private:
    int32_t _post_write(ScatterGatherElement && elems, ibv_send_wr wr, bool force_inline);
  };
}

#endif

