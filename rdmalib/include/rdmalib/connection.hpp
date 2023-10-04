
#ifndef __RDMALIB_CONNECTION_HPP__
#define __RDMALIB_CONNECTION_HPP__

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include <optional>

#ifdef USE_LIBFABRIC
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fabric.h>
#include <rdma/fi_eq.h>
#else
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#endif

#include <spdlog/spdlog.h>

#include <rdmalib/queue.hpp>
#include <rdmalib/buffer.hpp>

namespace rdmalib {

  enum class QueueType{
    SEND,
    RECV
  };

  #ifndef USE_LIBFABRIC
  struct ConnectionConfiguration {
    // Configuration of QP
    ibv_qp_init_attr attr;
    rdma_conn_param conn_param;

    ConnectionConfiguration();
  };
  #endif

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
    #ifdef USE_LIBFABRIC
    fid_ep* _qp;
    fid_cq* _rcv_channel;
    fid_cq* _trx_channel;
    fid_cntr* _write_counter;
    uint64_t _counter;
    #else
    rdma_cm_id* _id;
    ibv_qp* _qp; 
    ibv_comp_channel* _channel;
    #endif
    int32_t _req_count;
    int32_t _private_data;
    bool _passive;
    ConnectionStatus _status;

    // FIXME: associate this with RecvBuffer
    #ifdef USE_LIBFABRIC
    static const int _wc_size = 32; 
    std::array<fi_cq_data_entry, _wc_size> _swc; // fast fix for overlapping polling
    std::array<fi_cq_data_entry, _wc_size> _rwc;
    fi_cq_err_entry _ewc;
    std::array<ScatterGatherElement, _wc_size> _rwc_sges;
    #else
    SendWorkCompletions _send_wcs;
    RecvWorkCompletions _rcv_wcs;
    #endif
    int _send_flags;

    #ifndef USE_LIBFABRIC
    #endif

  public:
    Connection(int rcv_buf_size, bool passive = false);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&);

    void initialize_batched_recv(const rdmalib::impl::Buffer & sge, size_t offset);
    #ifndef USE_LIBFABRIC
    RecvWorkCompletions& receive_wcs();
    SendWorkCompletions& send_wcs();

    int rcv_buf_size() const;
    void inlining(bool enable);
    #endif
    #ifdef USE_LIBFABRIC
    void initialize(fid_fabric* fabric, fid_domain* pd, fi_info* info, fid_eq* ec, fid_cntr* write_cntr=nullptr, fid_cq* rx_channel=nullptr, fid_cq* tx_channel=nullptr);
    #else
    void initialize(rdma_cm_id* id);
    #endif
    void close();
    #ifdef USE_LIBFABRIC
    fid_domain* _domain = nullptr;
    fid* id() const;
    fid_ep* qp() const;
    fid_wait* wait_set() const;
    fid_cq* receive_completion_channel() const;
    fid_cq* transmit_completion_channel() const;
    #else
    rdma_cm_id* id() const;
    ibv_qp* qp() const;
    ibv_comp_channel* completion_channel() const;
    #endif
    uint32_t private_data() const;
    ConnectionStatus status() const;
    void set_status(ConnectionStatus status);
    void set_private_data(uint32_t private_data);

    // Blocking, no timeout
    #ifdef USE_LIBFABRIC
    std::tuple<fi_cq_data_entry*, int> poll_wc(QueueType, bool blocking = true, int count = -1, bool update = false);
    #else
    std::tuple<ibv_wc*, int> poll_wc(QueueType, bool blocking = true, int count = -1);
    #endif
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
    #ifdef USE_LIBFABRIC
    template<typename T> inline int32_t post_write(const Buffer<T> & buf, const size_t size, const uint64_t offset, const RemoteBuffer & rbuf, const uint32_t immediate) {
      int ret = fi_writedata(_qp, (void *)(buf.address() + offset), size, buf.lkey(), immediate + (size << 32), NULL, rbuf.addr, rbuf.rkey, (void *)(_req_count++));
      if(ret) {
        spdlog::error("Post write unsuccessful, reason {} {}, buf size {}, id {}, remote addr {}, remote rkey {}, imm data {}, connection {}",
          ret, strerror(ret), size, _req_count,  rbuf.addr, rbuf.rkey, immediate + (size << 32), fmt::ptr(this)
        );
        return -1;
      }
      if(size > 0)
        SPDLOG_DEBUG(
            "Post write succesfull id: {}, buf size: {}, lkey {}, remote addr {}, remote rkey {}, imm data {}, connection {}",
            _req_count, buf.bytes(), fmt::ptr(buf.lkey()), rbuf.addr, rbuf.rkey, immediate + (size << 32), fmt::ptr(this)
        );
      else
        SPDLOG_DEBUG(
            "Post write succesfull id: {}, remote addr {}, remote rkey {}, imm data {}, connection {}", _req_count,  rbuf.addr, rbuf.rkey, immediate + (size << 32), fmt::ptr(this)
        );
      return _req_count - 1;
    }
    int32_t post_atomic_fadd(const Buffer<uint64_t> & _accounting_buf, const RemoteBuffer & rbuf, uint64_t add);
    #else
    int32_t post_atomic_fadd(ScatterGatherElement && elems, const RemoteBuffer & rbuf, uint64_t add);
    #endif

    // Register to be notified about all events, including unsolicited ones
    #ifdef USE_LIBFABRIC
    int wait_events(int timeout = -1);
    #else
    void notify_events(bool only_solicited = false);
    ibv_cq* wait_events();
    void ack_events(ibv_cq* cq, int len);
    #endif
  private:
    #ifdef USE_LIBFABRIC
    int32_t _post_write(ScatterGatherElement && elems, const RemoteBuffer & rbuf, const uint32_t immediate = 0);
    #else
    int32_t _post_write(ScatterGatherElement && elems, ibv_send_wr wr, bool force_inline, bool force_solicited);
    #endif
  };
}

#endif

