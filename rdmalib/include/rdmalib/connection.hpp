
#ifndef __RDMALIB_CONNECTION_HPP__
#define __RDMALIB_CONNECTION_HPP__

#include <rdma/fabric.h>
#include <rdma/fi_eq.h>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include <optional>

//#ifdef USE_LIBFABRIC
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <spdlog/spdlog.h>
//#else
#include <infiniband/verbs.h>

#include <rdma/rdma_cma.h>
//#endif
#include <rdmalib/buffer.hpp>
#include <rdmalib/libraries.hpp>

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

  // State of a communication:
  // a) communication ID
  // b) Queue Pair
  template <typename Derived, typename Library>
  struct Connection {
  protected:
    // Bring types into scope
    using qp_t      = typename library_traits<Library>::qp_t;
    using wc_t      = typename library_traits<Library>::wc_t;
    using id_t      = typename library_traits<Library>::id_t;
    using channel_t = typename library_traits<Library>::channel_t;
    template <typename S> // TODO: remove this generic. should be a trait
    using SGE = ScatterGatherElement<S, Library>;
    using RemoteBuffer_ = RemoteBuffer<Library>;

    qp_t _qp; 
    int32_t _req_count;
    int32_t _private_data;
    bool _passive;
    ConnectionStatus _status;
    static const int _wc_size = 32; 
    // FIXME: associate this with RecvBuffer
    std::array<wc_t, _wc_size> _swc; // fast fix for overlapping polling
    std::array<wc_t, _wc_size> _rwc;

    int _send_flags;

  public:
    static const int _rbatch = 32; // 32 for faster division in the code

    Connection(bool passive = false) {
      
    }
    ~Connection() {
      
    }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&);

    template <typename B>
    void initialize_batched_recv(const rdmalib::impl::Buffer<B, Library> & sge, size_t offset);
    void close()
    {
      static_cast<Derived*>(this)->close();
    }
    id_t id() const
    {
      return static_cast<Derived*>(this)->id();
    }
    qp_t qp() const
    {
      return this->_qp; // TODO sure?
    }

    uint32_t private_data() const;
    ConnectionStatus status() const;
    void set_status(ConnectionStatus status);
    void set_private_data(uint32_t private_data);

    // Blocking, no timeout

    std::tuple<wc_t, int> poll_wc(QueueType, bool blocking = true, int count = -1, bool update = false);

    template <typename S>
    int32_t post_send(const SGE<S> & elem, int32_t id = -1, bool force_inline = false);
    template <typename S>
    int32_t post_recv(SGE<S> && elem, int32_t id = -1, int32_t count = 1);

    int32_t post_batched_empty_recv(int32_t count = 1);
    template <typename S>
    int32_t post_write(SGE<S> && elems, const RemoteBuffer_ & buf, bool force_inline = false);
    // Solicited makes sense only for RDMA write with immediate
    template <typename S>
    int32_t post_write(SGE<S> && elems, const RemoteBuffer_ & buf,
      uint32_t immediate,
      bool force_inline = false,
      bool solicited = false
    );
    template <typename S>
    int32_t post_cas(SGE<S> && elems, const RemoteBuffer_ & buf, uint64_t compare, uint64_t swap);
  };

  struct LibfabricConnection : Connection<LibfabricConnection, libfabric>
  {
    template <typename T>
    using Buffer = Buffer<T, libfabric>;
    //using RemoteBuffer_ = RemoteBuffer_<libfabric>;
    using SGE = LibfabricScatterGatherElement;

    fid_cq *_rcv_channel;
    fid_cq *_trx_channel;
    fid_cntr* _write_counter;
    uint64_t _counter;
    fid_domain* _domain = nullptr;

    std::array<SGE, _wc_size> _rwc_sges;

    fi_cq_err_entry _ewc;

    LibfabricConnection(bool passive=false);
    LibfabricConnection(LibfabricConnection&& obj);
    ~LibfabricConnection();

    id_t id() const
    {
      return &this->_qp->fid;
    }

    template <typename B>
    void initialize_batched_recv(const rdmalib::impl::Buffer<B, libfabric> & sge, size_t offset);

    void initialize(fid_fabric* fabric, fid_domain* pd, fi_info* info, fid_eq* ec, fid_cntr* write_cntr, fid_cq* rx_channel, fid_cq* tx_channel);
    void close();

    fid_wait* wait_set() const;
    channel_t receive_completion_channel() const;
    channel_t transmit_completion_channel() const;

    int32_t post_cas(SGE && elems, const RemoteBuffer_ & rbuf, uint64_t compare, uint64_t swap);
    int32_t post_send(const SGE & elems, int32_t id, bool force_inline);
    int32_t post_batched_empty_recv(int count);
    int32_t post_recv(SGE && elem, int32_t id, int count);

    template<typename T> inline int32_t post_write(const Buffer<T> & buf, const size_t size, const uint64_t offset, const RemoteBuffer_ & rbuf, const uint32_t immediate) {
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

    int32_t post_atomic_fadd(const Buffer<uint64_t> & _accounting_buf, const RemoteBuffer_ & rbuf, uint64_t add);

    // Register to be notified about all events, including unsolicited ones
    int wait_events(int timeout = -1);

    int32_t _post_write(SGE && elems, const RemoteBuffer_ & rbuf, const uint32_t immediate = 0);
    int32_t post_write(SGE && elems, const RemoteBuffer_ & rbuf, bool force_inline);

    std::tuple<fi_cq_data_entry *, int> poll_wc(QueueType type, bool blocking=true, int count=-1, bool update=false);
  };

  struct VerbsConnection : Connection<VerbsConnection, ibverbs>
  {
    using SGE = VerbsScatterGatherElement;
    // using RemoteBuffer_ = RemoteBuffer_<ibverbs>; // handled in parent

    id_t _id;
    channel_t _channel;

    struct ibv_recv_wr _batch_wrs[_rbatch]; // preallocated and prefilled batched recv.
    std::array<SGE, _wc_size> _rwc_sges;

    VerbsConnection(bool passive=false);
    VerbsConnection(VerbsConnection&& obj);
    ~VerbsConnection();
    void close();

    id_t id() const
    {
      return this->_id;
    }

    void inlining(bool enable);
    template <typename B>
    void initialize_batched_recv(const rdmalib::impl::Buffer<B, ibverbs> & sge, size_t offset);
    void initialize(rdma_cm_id* id);
    ibv_comp_channel* completion_channel() const;

    int32_t post_send(const SGE & elems, int32_t id, bool force_inline);
    int32_t post_batched_empty_recv(int count);
    int32_t post_recv(SGE && elem, int32_t id, int count);
    int32_t post_cas(SGE && elems, const RemoteBuffer_ & rbuf, uint64_t compare, uint64_t swap);
    int32_t post_atomic_fadd(SGE && elems, const RemoteBuffer_ & rbuf, uint64_t add);

    void notify_events(bool only_solicited = false);
    ibv_cq* wait_events();
    void ack_events(ibv_cq* cq, int len);

    int32_t _post_write(SGE && elems, ibv_send_wr wr, bool force_inline, bool force_solicited);
    int32_t post_write(SGE && elems, const RemoteBuffer_ & rbuf, bool force_inline);

    std::tuple<ibv_wc*, int> poll_wc(QueueType type, bool blocking=true, int count=-1);

  };
}

#endif
