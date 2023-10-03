
#ifndef __RDMALIB_CONNECTION_HPP__
#define __RDMALIB_CONNECTION_HPP__

#include "rdmalib/queue.hpp"
#include <cstdint>
#include <initializer_list>
#include <vector>
#include <optional>

#include <infiniband/verbs.h>

#include <rdma/rdma_cma.h>
#include <rdmalib/buffer.hpp>

namespace rdmalib {

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

  enum class QueueType{
    SEND,
    RECV
  };

  template<int Key = 8, int UserData = 8, int Secret = 16>
  struct PrivateData {

    PrivateData(uint32_t private_data = 0):
      _private_data(private_data)
    {}

    uint32_t secret() const
    {
      // Extract last Key bits, then shift to lower bits.
      uint32_t pattern = (static_cast<uint32_t>(1) << Secret) - 1;
      return (this->_private_data & pattern);
    }

    void secret(uint32_t value)
    {
      // Extract first Key bits, then store at higher bits.
      uint32_t pattern = (static_cast<uint32_t>(1) << Secret) - 1;
      _private_data |= (value & pattern);
    }

    uint32_t key() const
    {
      // Extract last Key bits, then shift to lower bits.
      uint32_t pattern = ((static_cast<uint32_t>(1) << Key) - 1) << (UserData + Secret);
      return (this->_private_data & pattern) >> (UserData + Secret);
    }

    void key(uint32_t value)
    {
      // Extract first Key bits, then store at higher bits.
      uint32_t pattern = (static_cast<uint32_t>(1) << Key) - 1;
      _private_data |= (value & pattern) << (UserData + Secret);
    }

    uint32_t user_data() const
    {
      // Extract last Key bits, then shift to lower bits.
      uint32_t pattern = ((static_cast<uint32_t>(1) << UserData) - 1) << (Secret);
      return (this->_private_data & pattern) >> (Secret);
    }

    void user_data(uint32_t value)
    {
      // Extract first Key bits, then store at higher bits.
      uint32_t pattern = (static_cast<uint32_t>(1) << UserData) - 1;
      _private_data |= (value & pattern) << (Secret);
    }

    uint32_t data() const
    {
      return _private_data;
    }

  private:
    uint32_t _private_data;
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

    SendWorkCompletions _send_wcs;
    RecvWorkCompletions _rcv_wcs;
    int _send_flags;

  public:
    Connection(int rcv_buf_size, bool passive = false);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&);

    RecvWorkCompletions& receive_wcs();
    SendWorkCompletions& send_wcs();

    int rcv_buf_size() const;

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

