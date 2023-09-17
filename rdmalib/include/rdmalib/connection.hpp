
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

  enum class ConnectionStatus {
    // The connection object does not bind to a defined RDMA connection.
    UNKNOWN = 0,
    // The connection has been requested, RDMA objects are allocated, but connection is not ready.
    REQUESTED,
    // The connection has been established and can be used.
    ESTABLISHED,
    // The connection has been disconnected and mustn't be used.
    DISCONNECTED
  };

  // State of a communication:
  // a) communication ID
  // b) Queue Pair
  struct Connection {
  private:
    rdma_cm_id* _id;
    ibv_qp* _qp; 
    ibv_comp_channel* _channel;
    int32_t _req_count;
    int32_t _private_data;
    bool _passive;
    ConnectionStatus _status;
    static const int _wc_size = 32; 
    // FIXME: associate this with RecvBuffer
    std::array<ibv_wc, _wc_size> _swc; // fast fix for overlapping polling
    std::array<ibv_wc, _wc_size> _rwc;
    std::array<ScatterGatherElement, _wc_size> _rwc_sges;
    int _send_flags;

    static const int _rbatch = 32; // 32 for faster division in the code
    struct ibv_recv_wr _batch_wrs[_rbatch]; // preallocated and prefilled batched recv.

  public:
    Connection(bool passive = false);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&);

    void initialize_batched_recv(const rdmalib::impl::Buffer & sge, size_t offset);
    void inlining(bool enable);
    void initialize(rdma_cm_id* id);
    void close();
    rdma_cm_id* id() const;
    ibv_qp* qp() const;
    ibv_comp_channel* completion_channel() const;
    uint32_t private_data() const;
    ConnectionStatus status() const;
    void set_status(ConnectionStatus status);
    void set_private_data(uint32_t private_data);

    // Blocking, no timeout
    std::tuple<ibv_wc*, int> poll_wc(QueueType, bool blocking = true, int count = -1);
    int32_t post_send(const ScatterGatherElement & elem, int32_t id = -1, bool force_inline = false, std::optional<uint32_t> immediate = std::nullopt);
    int32_t post_recv(ScatterGatherElement && elem, int32_t id = -1, int32_t count = 1);

    int32_t post_batched_empty_recv(int32_t count = 1);

    int32_t post_write(ScatterGatherElement && elems, const RemoteBuffer & buf, bool force_inline = false);
    // Solicited makes sense only for RDMA write with immediate
    int32_t post_write(ScatterGatherElement && elems, const RemoteBuffer & buf,
      uint32_t immediate,
      bool force_inline = false,
      bool solicited = false
    );
    int32_t post_cas(ScatterGatherElement && elems, const RemoteBuffer & buf, uint64_t compare, uint64_t swap);
    int32_t post_atomic_fadd(ScatterGatherElement && elems, const RemoteBuffer & rbuf, uint64_t add);

    // Register to be notified about all events, including unsolicited ones
    void notify_events(bool only_solicited = false);
    ibv_cq* wait_events();
    void ack_events(ibv_cq* cq, int len);
  private:
    int32_t _post_write(ScatterGatherElement && elems, ibv_send_wr wr, bool force_inline, bool force_solicited);
  };
}

#endif

