
#include <asm-generic/errno-base.h>
#include <cstddef>
#include <cstdint>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <chrono>
// #ifdef USE_LIBFABRIC
#include <rdma/fabric.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#include "rdmalib/buffer.hpp"
#include <arpa/inet.h>
#include <rdma/fi_atomic.h>
// #endif
#include <spdlog/spdlog.h>
#include <thread>

#include <rdmalib/connection.hpp>
#include <rdmalib/util.hpp>

namespace rdmalib {

  #ifndef USE_LIBFABRIC
  ConnectionConfiguration::ConnectionConfiguration()
  {
    memset(&attr, 0, sizeof(attr));
    memset(&conn_param, 0 , sizeof(conn_param));
  }
  #endif

  LibfabricConnection::LibfabricConnection(bool passive)
  {
    _qp = nullptr;
    _rcv_channel = nullptr;
    _trx_channel = nullptr;
    _write_counter = nullptr;
    _req_count = 0;
    _private_data = 0;
    _passive = passive;
    _status= ConnectionStatus::UNKNOWN;
    SPDLOG_DEBUG("Allocate a connection {}", fmt::ptr(this));
  }
  
  VerbsConnection::VerbsConnection(bool passive)
  {
    _qp = nullptr;
    _id = nullptr;
    _channel = nullptr;
    _req_count = 0;
    _private_data = 0;
    _passive = passive;
    _status = ConnectionStatus::UNKNOWN;

    inlining(false);

    for(int i=0; i < _rbatch; i++){
      _batch_wrs[i].wr_id = i;
      _batch_wrs[i].sg_list = 0;
      _batch_wrs[i].num_sge = 0;
      _batch_wrs[i].next=&(_batch_wrs[i+1]);
    }
    _batch_wrs[_rbatch-1].next = NULL;
    SPDLOG_DEBUG("Allocate a connection with id {}", fmt::ptr(_id));
  }

  LibfabricConnection::~LibfabricConnection()
  {
    SPDLOG_DEBUG("Deallocate connection {} with qp fid {}", fmt::ptr(this), fmt::ptr(&_qp->fid));
    close();
  }

  VerbsConnection::~VerbsConnection()
  {
    SPDLOG_DEBUG("Deallocate a connection with id {}", fmt::ptr(_id));
    close();
  }

  LibfabricConnection::LibfabricConnection(LibfabricConnection&& obj)
  {
    _qp = obj._qp;
    _rcv_channel = obj._rcv_channel;
    _trx_channel = obj._trx_channel;
    _write_counter = nullptr;
    _req_count = obj._req_count;
    _private_data = obj._private_data;
    _passive = obj._passive;
    _status = obj._status;
    _send_flags = obj._send_flags;
    
    obj._qp = nullptr;
    obj._req_count = 0;
  }

  VerbsConnection::VerbsConnection(VerbsConnection&& obj)
  {
    _qp = obj._qp;
    _id = obj._id;
    _channel = obj._channel;
    _req_count = obj._req_count;
    _private_data = obj._private_data;
    _passive = obj._passive;
    _status = obj._status;
    _send_flags = obj._send_flags;
    obj._id = nullptr;
    obj._qp = nullptr;
    obj._req_count = 0;

    for(int i=0; i < _rbatch; i++){
      _batch_wrs[i].wr_id = i;
      _batch_wrs[i].sg_list = 0;
      _batch_wrs[i].num_sge = 0;
      _batch_wrs[i].next=&(_batch_wrs[i+1]);
    }
    _batch_wrs[_rbatch-1].next = NULL;
  }


  template <typename B>
  void LibfabricConnection::initialize_batched_recv(const rdmalib::impl::Buffer<B, libfabric> & buf, size_t offset)
  {
    for(int i = 0; i < _rbatch; i++){
      _rwc_sges[i] = buf.sge(offset, i*offset);
      //for(auto & sg : _rwc_sges[i]._sges)
      //sg.addr += i*offset;
    }
  }

  template <typename B>
  void VerbsConnection::initialize_batched_recv(const rdmalib::impl::Buffer<B, ibverbs> & buf, size_t offset)
  {
    for(int i = 0; i < _rbatch; i++){
      _rwc_sges[i] = buf.sge(offset, i*offset);
      //for(auto & sg : _rwc_sges[i]._sges)
      //sg.addr += i*offset;
      _batch_wrs[i].sg_list = _rwc_sges[i].array();
      _batch_wrs[i].num_sge = _rwc_sges[i].size();
    }
  }

  void LibfabricConnection::initialize(fid_fabric* fabric, fid_domain* pd, fi_info* info, fid_eq* ec, fid_cntr* write_cntr, fid_cq* rx_channel, fid_cq* tx_channel)
  {
    // Create the endpoint and set its flags up so that we get completions on RDM
    impl::expect_zero(fi_endpoint(pd, info, &_qp, reinterpret_cast<void*>(this)));

    // Bind the counter for write operations
    _write_counter = write_cntr;
    impl::expect_zero(fi_ep_bind(_qp, &_write_counter->fid, FI_REMOTE_WRITE));

    // Bind with the completion queues and the event queue
    impl::expect_zero(fi_ep_bind(_qp, &ec->fid, 0));
    _trx_channel = tx_channel;
    _rcv_channel = rx_channel;
    impl::expect_zero(fi_ep_bind(_qp, &_trx_channel->fid, FI_TRANSMIT));
    impl::expect_zero(fi_ep_bind(_qp, &_rcv_channel->fid, FI_RECV));

    // Enable the endpoint
    impl::expect_zero(fi_enable(_qp));
    SPDLOG_DEBUG("Initialize connection {}", fmt::ptr(this));
  }

  void VerbsConnection::initialize(rdma_cm_id* id)
  {
    this->_id = id;
    this->_channel = _id->recv_cq_channel;
    this->_qp = this->_id->qp;
    SPDLOG_DEBUG("Initialize a connection with id {}", fmt::ptr(_id));
  }

  void VerbsConnection::inlining(bool enable)
  {
    if(enable)
      _send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
    else
      _send_flags = IBV_SEND_SIGNALED;
  }

  void LibfabricConnection::close()
  {
    SPDLOG_DEBUG("Connection close called for {} with qp fid {}", fmt::ptr(this), fmt::ptr(&this->_qp->fid));
    // We need to close the transmit and receive channels and the endpoint
    if (_status != ConnectionStatus::DISCONNECTED) {
      // TODO Check how to free those and if it's necessary at all.
      //      When closing the endpoint we obtain a corrupted double-linked list problem
      //      within gnix.
      // if (_rcv_channel) {
      //   impl::expect_zero(fi_close(&_rcv_channel->fid));
      //   _rcv_channel = nullptr;
      // }
      // if (_trx_channel) {
      //   impl::expect_zero(fi_close(&_trx_channel->fid));
      //   _trx_channel = nullptr;
      // }
      // if (_write_counter) {
      //   impl::expect_zero(fi_close(&_write_counter->fid));
      //   _write_counter = nullptr;
      // }
      // if (_qp) {
      //   impl::expect_zero(fi_shutdown(_qp, 0));
      //   impl::expect_zero(fi_close(&_qp->fid));
      //   _qp = nullptr;
      // }
      // if (_domain) {
      //   impl::expect_zero(fi_close(&_domain->fid));
      //   _domain = nullptr;
      // }
      _status = ConnectionStatus::DISCONNECTED;
    }
  }

  void VerbsConnection::close()
  {
    SPDLOG_DEBUG("Connection close called for {} id {}", fmt::ptr(this), fmt::ptr(this->_id));
    if(_id) {
      // When the connection is allocated on active side
      // We allocated ep, and that's the only thing we need to do
      if(!_passive) {
        SPDLOG_DEBUG("Connection active close destroy ep id {} qp {}", fmt::ptr(_id), fmt::ptr(_id->qp));
        rdma_destroy_qp(_id);
        rdma_destroy_ep(_id);
      }
      // When the connection is allocated on passive side
      // We allocated QP and we need to free an ID
      else {
        SPDLOG_DEBUG("Connection passive close destroy qp {}", fmt::ptr(_id->qp));
        rdma_destroy_qp(_id);
        SPDLOG_DEBUG("Connection passive close destroy id {}", fmt::ptr(_id));
        rdma_destroy_id(_id);
      }
      _id = nullptr;
      _status = ConnectionStatus::DISCONNECTED;
    }
  }

  id_t LibfabricConnection::id() const
  {
    return &this->_qp->fid;
  }
  id_t VerbsConnection::id() const
  {
    return this->_id;
  }

  qp_t LibfabricConnection::qp() const
  {
    return this->_qp;
  }
  qp_t VerbsConnection::qp() const
  {
    return this->_qp;
  }

  fid_cq* LibfabricConnection::receive_completion_channel() const
  {
    return this->_rcv_channel;
  }
  fid_cq* LibfabricConnection::transmit_completion_channel() const
  {
    return this->_trx_channel;
  }
  ibv_comp_channel* VerbsConnection::completion_channel() const
  {
    return this->_channel;
  }

  template <typename D, typename L>
  uint32_t Connection<D,L>::private_data() const
  {
    return this->_private_data;
  }

  template <typename D, typename L>
  ConnectionStatus Connection<D,L>::status() const
  {
    return this->_status;
  }

  template <typename D, typename L>
  void Connection<D,L>::set_status(ConnectionStatus status)
  {
    this->_status = status;
  }

  template <typename D, typename L>
  void Connection<D,L>::set_private_data(uint32_t private_data)
  {
    this->_private_data = private_data;
  }

  int32_t LibfabricConnection::post_send(const ScatterGatherElement & elems, int32_t id, bool force_inline)
  {
    // FIXME: extend with multiple sges
    id = id == -1 ? _req_count++ : id;
    SPDLOG_DEBUG("Post send to local Local QPN on connection {} fid {}", fmt::ptr(this), fmt::ptr(&_qp->fid));
    int ret = fi_sendv(_qp, elems.array(), elems.lkeys(), elems.size(), NULL, reinterpret_cast<void *>((uint64_t)id));
    if(ret) {
      spdlog::error("Post send unsuccessful on connection {} reason {} message {} errno {} message {}, sges_count {}, wr_id {}",
        fmt::ptr(this), ret, fi_strerror(std::abs(ret)), errno, strerror(errno), elems.size(), id
      );
      return -1;
    }
    SPDLOG_DEBUG(
      "Post send successful on connection {}, sges_count {}, sge[0].addr {}, sge[0].size {}, wr_id {}",
      fmt::ptr(this), elems.size(), elems.array()[0].iov_base, elems.array()[0].iov_len, id
    );
    return _req_count - 1;
  }
  
  int32_t VerbsConnection::post_send(const ScatterGatherElement & elems, int32_t id, bool force_inline)
  {
    // FIXME: extend with multiple sges
    struct ibv_send_wr wr, *bad;
    wr.wr_id = id == -1 ? _req_count++ : id;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = force_inline ? IBV_SEND_SIGNALED | IBV_SEND_INLINE : _send_flags;
    SPDLOG_DEBUG("Post send to local Local QPN {}",_qp->qp_num);
    int ret = ibv_post_send(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post send unsuccesful, reason {} {}, sges_count {}, wr_id {}, wr.send_flags {}",
        errno, strerror(errno), wr.num_sge, wr.wr_id, wr.send_flags
      );
      return -1;
    }
    SPDLOG_DEBUG(
      "Post send succesfull, sges_count {}, sge[0].addr {}, sge[0].size {}, wr_id {}, wr.send_flags {}",
      wr.num_sge, wr.sg_list[0].addr, wr.sg_list[0].length, wr.wr_id, wr.send_flags
    );
    return _req_count - 1;
  }

  int32_t LibfabricConnection::post_batched_empty_recv(int count)
  {
    int loops = count / _rbatch;
    int reminder = count % _rbatch;
    SPDLOG_DEBUG("Batch {} {} to local QPN on connection {} fid {}", loops, reminder, fmt::ptr(this), fmt::ptr(&_qp->fid));

    int ret = 0;
    for(int i = 0; i < loops; ++i) {
      for(int j = 0; j < _rbatch; ++j) {
        auto begin = _rwc_sges[j];
        for (size_t k = 0; k < begin.size(); ++k) {
          if(begin.array()[k].iov_len > 0) {
            SPDLOG_DEBUG("Batched receive on connection {} num_sge {} sge[0].ptr {} sge[0].length {}", fmt::ptr(this), begin.size(), begin.array()[k].iov_base, begin.array()[k].iov_len);
          } else
            SPDLOG_DEBUG("Batched receive on connection {} num_sge {}", fmt::ptr(this), begin.size());
        }
        ret = fi_recv(_qp, begin.array()->iov_base, begin.array()->iov_len, begin.lkeys()[0], NULL, reinterpret_cast<void *>(j));
        if(ret)
          break;
      }
      if(ret)
        break;
    }

    if(ret == 0 && reminder > 0){
      for(int j = 0; j < reminder; ++j) {
        auto begin = _rwc_sges[j];
        for (size_t k = 0; k < begin.size(); ++k) {
          if(begin.array()[k].iov_len > 0) {
            SPDLOG_DEBUG("Batched receive on connection {} num_sge {} sge[0].ptr {} sge[0].length {}", fmt::ptr(this), begin.size(), begin.array()[k].iov_base, begin.array()[k].iov_len);
          } else
            SPDLOG_DEBUG("Batched receive on connection {} num_sge {}", fmt::ptr(this), begin.size());
        }
        ret = fi_recv(_qp, begin.array()->iov_base, begin.array()->iov_len, begin.lkeys()[0], NULL, reinterpret_cast<void *>(j));
        if(ret)
          break;
      }
    }

    if(ret) {
      spdlog::error("Batched Post empty recv unsuccessful on connection {} reason {} {}", fmt::ptr(this), ret, fi_strerror(std::abs(ret)));
      return -1;
    }

    SPDLOG_DEBUG("Batched Post empty recv successfull on connection {}", fmt::ptr(this));
    return count;
  }
  
  int32_t VerbsConnection::post_batched_empty_recv(int count)
  {
    struct ibv_recv_wr* bad = nullptr;
    int loops = count / _rbatch;
    int reminder = count % _rbatch;
    SPDLOG_DEBUG("Batch {} {} to local QPN {}", loops, reminder, _qp->qp_num);

    int ret = 0;
    for(int i = 0; i < loops; ++i) {
      auto begin = &_batch_wrs[0];
      while(begin) {
        if(begin->num_sge > 0) {
          SPDLOG_DEBUG("Batched receive num_sge {} sge[0].ptr {} sge[0].length {}", begin->num_sge, begin->sg_list[0].addr, begin->sg_list[0].length);
        } else
          SPDLOG_DEBUG("Batched receive num_sge {}", begin->num_sge);
        begin = begin->next;
      }
      ret = ibv_post_recv(_qp, &_batch_wrs[0], &bad);
      if(ret)
        break;
    }

    if(ret == 0 && reminder > 0){
      _batch_wrs[reminder-1].next=NULL;
      auto begin = &_batch_wrs[0];
      while(begin) {
        if(begin->num_sge > 0) {
          SPDLOG_DEBUG("Batched receive num_sge {} sge[0].ptr {} sge[0].length {}", begin->num_sge, begin->sg_list[0].addr, begin->sg_list[0].length);
        } else {
          SPDLOG_DEBUG("Batched receive num_sge {}", begin->num_sge);
	      }
        begin = begin->next;
      }
      ret = ibv_post_recv(_qp, _batch_wrs, &bad);
      _batch_wrs[reminder-1].next= &(_batch_wrs[reminder]);
    }

    if(ret) {
      spdlog::error("Batched Post empty recv  unsuccesful, reason {} {}", ret, strerror(ret));
      return -1;
    }

    SPDLOG_DEBUG("Batched Post empty recv succesfull");
    return count;
  }

  int32_t LibfabricConnection::post_recv(ScatterGatherElement && elem, int32_t id, int count)
  {
    fi_addr_t temp = 0;
    id = id == -1 ? _req_count++ : id;
    SPDLOG_DEBUG("post recv to local Local QPN fid {} connection {}", fmt::ptr(&_qp->fid), fmt::ptr(this));

    int ret = 1;
    for(int i = 0; i < count; ++i) {
      ret = fi_recvv(_qp, elem.array(), elem.lkeys(), count, temp, reinterpret_cast<void *>((uint64_t)id));
      if(ret)
        break;
    }
    if(ret) {
      spdlog::error("Post receive unsuccessful on connection {}, reason {} {}", fmt::ptr(this), ret, strerror(ret));
      return -1;
    }
    if(elem.size() > 0)
      SPDLOG_DEBUG(
        "Post recv successfull on connection {}, sges_count {}, sge[0].addr {}, sge[0].size {}, wr_id {}",
        fmt::ptr(this), elem.size(), elem.array()[0].iov_base, elem.array()[0].iov_len, id
      );
    else
      SPDLOG_DEBUG("Post recv successfull on connection {}", fmt::ptr(this));
    return id;
  }

  int32_t VerbsConnection::post_recv(ScatterGatherElement && elem, int32_t id, int count)
  {
    // FIXME: extend with multiple sges
    struct ibv_recv_wr wr, *bad;
    wr.wr_id = id == -1 ? _req_count++ : id;
    wr.next = nullptr;
    wr.sg_list = elem.array();
    wr.num_sge = elem.size();
    SPDLOG_DEBUG("post recv to local Local QPN {}",_qp->qp_num);

    int ret = 1;
    for(int i = 0; i < count; ++i) {
      ret = ibv_post_recv(_qp, &wr, &bad);
      if(ret)
        break;
    }
    if(ret) {
      spdlog::error("Post receive unsuccesful, reason {} {}", ret, strerror(ret));
      return -1;
    }
    if(wr.num_sge > 0)
      SPDLOG_DEBUG(
        "Post recv succesfull, sges_count {}, sge[0].addr {}, sge[0].size {}, wr_id {}",
        wr.num_sge, wr.sg_list[0].addr, wr.sg_list[0].length, wr.wr_id
      );
    else
      SPDLOG_DEBUG("Post recv succesfull");
    return wr.wr_id;
  }

  int32_t LibfabricConnection::_post_write(ScatterGatherElement && elems, const RemoteBuffer & rbuf, const uint32_t immediate)
  {
    fi_addr_t temp = 0;
    int32_t id = _req_count++;
    size_t count = elems.size();
    uint64_t data = immediate + (elems.array()[0].iov_len << 32);
    int ret = fi_writedata(_qp, elems.array()[0].iov_base, elems.array()[0].iov_len, elems.lkeys()[0], data, temp, rbuf.addr, rbuf.rkey, reinterpret_cast<void *>((uint64_t)id));
    if(ret) {
      spdlog::error("Post write unsuccessful, reason {} {}, sges_count {}, wr_id {}, remote addr {}, remote rkey {}, imm data {}, connection {}",
        ret, strerror(ret), count, id,  rbuf.addr, rbuf.rkey, data, fmt::ptr(this)
      );
      return -1;
    }
    if(elems.size() > 0)
      SPDLOG_DEBUG(
          "Post write succesfull id: {}, sge size: {}, first lkey {} len {}, remote addr {}, remote rkey {}, imm data {}, connection {}",
          id, count, elems.lkeys()[0], elems.array()[0].iov_len, rbuf.addr, rbuf.rkey, data, fmt::ptr(this)
      );
    else
      SPDLOG_DEBUG(
          "Post write succesfull id: {}, remote addr {}, remote rkey {}, imm data {}, connection {}", id,  rbuf.addr, rbuf.rkey, data, fmt::ptr(this)
      );
    return _req_count - 1;

  }

  int32_t VerbsConnection::_post_write(ScatterGatherElement && elems, ibv_send_wr wr, bool force_inline, bool force_solicited)
  {
    ibv_send_wr* bad;
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.send_flags = force_inline ? IBV_SEND_SIGNALED | IBV_SEND_INLINE : _send_flags;
    wr.send_flags = force_solicited ? IBV_SEND_SOLICITED | wr.send_flags : wr.send_flags;

    if(wr.num_sge == 1 && wr.sg_list[0].length == 0)
      wr.num_sge = 0;

    int ret = ibv_post_send(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post write unsuccesful, reason {} {}, sges_count {}, wr_id {}, remote addr {}, remote rkey {}, imm data {}",
        ret, strerror(ret), wr.num_sge, wr.wr_id,  wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, ntohl(wr.imm_data)
      );
      if(IBV_SEND_INLINE & wr.send_flags)
        spdlog::error("The write of size {} was inlined, is it supported by the device?",
          wr.sg_list[0].length
        );
      return -1;
    }
    if(wr.num_sge > 0)
      SPDLOG_DEBUG(
          "Post write succesfull id: {}, sge size: {}, first lkey {} len {}, remote addr {}, remote rkey {}, imm data {}",
          wr.wr_id, wr.num_sge, wr.sg_list[0].lkey, wr.sg_list[0].length, wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, ntohl(wr.imm_data)
      );
    else
      SPDLOG_DEBUG(
          "Post write succesfull id: {}, remote addr {}, remote rkey {}, imm data {}", wr.wr_id,  wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, ntohl(wr.imm_data)
      );
    return _req_count - 1;

  }

  int32_t LibfabricConnection::post_write(ScatterGatherElement && elems, const RemoteBuffer & rbuf, bool force_inline)
  {
    if (elems.size() > 1) {
      spdlog::error("Post write unsuccessful on connection {}, reason Function not implemented for multiple sges.", fmt::ptr(this));
      return -1;
    }
    return _post_write(std::forward<ScatterGatherElement>(elems), rbuf);
  }

  int32_t VerbsConnection::post_write(ScatterGatherElement && elems, const RemoteBuffer & rbuf, bool force_inline)
  {
    ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.wr.rdma.remote_addr = rbuf.addr;
    wr.wr.rdma.rkey = rbuf.rkey;
    return _post_write(std::forward<ScatterGatherElement>(elems), wr, force_inline, false);
  }

  int32_t LibfabricConnection::post_cas(ScatterGatherElement && elems, const RemoteBuffer & rbuf, uint64_t compare, uint64_t swap)
  {
    // TODO check if 
    fi_addr_t temp = 0;
    int32_t id = _req_count++;
    memcpy(elems.array()[0].iov_base, &swap, sizeof(swap));
    memcpy(elems.array()[1].iov_base, &compare, sizeof(compare));
    int ret = fi_compare_atomic(_qp, elems.array()[0].iov_base, 1, elems.lkeys()[0], elems.array()[1].iov_base, elems.lkeys()[1], elems.array()[1].iov_base, elems.lkeys()[1], temp, rbuf.addr, rbuf.rkey, FI_UINT64, FI_CSWAP, reinterpret_cast<void *>((uint64_t)id));
    if(ret) {
      spdlog::error("Post write unsuccessful on connection {}, reason {} {}", fmt::ptr(this), errno, strerror(errno));
      return -1;
    }
    SPDLOG_DEBUG("Post write id {} successful on connection", id, fmt::ptr(this));
    return _req_count - 1;
  }

  int32_t VerbsConnection::post_cas(ScatterGatherElement && elems, const RemoteBuffer & rbuf, uint64_t compare, uint64_t swap)
  {
    ibv_send_wr wr, *bad;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.opcode = IBV_WR_ATOMIC_CMP_AND_SWP;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.atomic.remote_addr = rbuf.addr;
    wr.wr.atomic.rkey = rbuf.rkey;
    wr.wr.atomic.compare_add = compare;
    wr.wr.atomic.swap = swap;

    int ret = ibv_post_send(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post write unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    SPDLOG_DEBUG("Post write succesfull");
    return _req_count - 1;
  }

  int32_t LibfabricConnection::post_atomic_fadd(const Buffer<uint64_t> & _accounting_buf, const RemoteBuffer & rbuf, uint64_t add)
  {
    int32_t id = _req_count++;
    memcpy(_accounting_buf.data(), &add, sizeof(add));
    int ret = fi_atomic(_qp, _accounting_buf.data(), 1, _accounting_buf.lkey(), NULL, rbuf.addr, rbuf.rkey, FI_UINT64, FI_SUM, reinterpret_cast<void *>((uint64_t)id));
    if(ret) {
      spdlog::error("Post write unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    SPDLOG_DEBUG(
        "Post atomic fadd succesfull id: {}, remote addr {}, remote rkey {}, val {}, connection {}", id, rbuf.addr, rbuf.rkey, *_accounting_buf.data(), fmt::ptr(this)
    );
    return _req_count - 1;
  }

  int32_t VerbsConnection::post_atomic_fadd(ScatterGatherElement && elems, const RemoteBuffer & rbuf, uint64_t add)
  {
    ibv_send_wr wr, *bad;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.atomic.remote_addr = rbuf.addr;
    wr.wr.atomic.rkey = rbuf.rkey;
    wr.wr.atomic.compare_add = add;

    int ret = ibv_post_send(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post write unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    SPDLOG_DEBUG(
        "Post atomic fadd succesfull id: {}, remote addr {}, remote rkey {}, val {}", wr.wr_id,  wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, wr.wr.atomic.compare_add
    );
    return _req_count - 1;
  }

  std::tuple<fi_cq_data_entry *, int> LibfabricConnection::poll_wc(QueueType type, bool blocking, int count, bool update)
  {
    int ret = 0;
    fi_cq_data_entry* wcs = (type == QueueType::RECV ? _rwc.data() : _swc.data());

    //spdlog::error("{} {} {}", fmt::ptr(_qp), fmt::ptr(_qp->recv_cq), fmt::ptr(wcs));
    do {
      ret = fi_cq_read(
        type == QueueType::RECV ? _rcv_channel : _trx_channel,
        wcs,
        count == -1 ? _wc_size : count
      );
      if (ret == -FI_EAVAIL) {
        ret = fi_cq_readerr(type == QueueType::RECV ? _rcv_channel : _trx_channel, &_ewc, 0);
        if (ret != 1)
          ret = -1;
        else
          spdlog::error(
              "Queue {} connection {} WC {} finished with an error {}",
              type == QueueType::RECV ? "recv" : "send", fmt::ptr(this),
              reinterpret_cast<uint64_t>(_ewc.op_context),
              fi_strerror(_ewc.err)
            );
      }
    } while(blocking && (ret == -EAGAIN || ret == 0));

    if(ret < 0 && ret != -EAGAIN) {
      spdlog::error("Failure of polling events from: {} queue connection {}! Return value {} message {} errno {}", type == QueueType::RECV ? "recv" : "send", fmt::ptr(this), ret, fi_strerror(std::abs(ret)), errno);
      return std::make_tuple(nullptr, -1);
    }
    if(ret > 0) {
      if (update)
        _counter += ret;
      for(int i = 0; i < ret; ++i) {
        SPDLOG_DEBUG("Connection {} Queue {} Ret {}/{} WC {}", fmt::ptr(this), type == QueueType::RECV ? "recv" : "send", i + 1, ret, reinterpret_cast<uint64_t>(wcs[i].op_context));
      }
    }
    return std::make_tuple(wcs, ret == -EAGAIN ? 0 : ret);
  }
  
  std::tuple<ibv_wc*, int> VerbsConnection::poll_wc(QueueType type, bool blocking, int count)
  {
    int ret = 0;
    ibv_wc* wcs = (type == QueueType::RECV ? _rwc.data() : _swc.data());

    //spdlog::error("{} {} {}", fmt::ptr(_qp), fmt::ptr(_qp->recv_cq), fmt::ptr(wcs));
    do {
      ret = ibv_poll_cq(
        type == QueueType::RECV ? _qp->recv_cq : _qp->send_cq,
        count == -1 ? _wc_size : count,
        wcs
      );
    } while(blocking && ret == 0);

    if(ret < 0) {
      spdlog::error("Failure of polling events from: {} queue! Return value {}, errno {}", type == QueueType::RECV ? "recv" : "send", ret, errno);
      return std::make_tuple(nullptr, -1);
    }
    if(ret)
      for(int i = 0; i < ret; ++i) {
        if(wcs[i].status != IBV_WC_SUCCESS) {
          spdlog::error(
            "Queue {} Work Completion {}/{} finished with an error {}, {}",
            type == QueueType::RECV ? "recv" : "send",
            i+1, ret, wcs[i].status, ibv_wc_status_str(wcs[i].status)
          );
        }
        SPDLOG_DEBUG("Queue {} Ret {}/{} WC {} Status {}", type == QueueType::RECV ? "recv" : "send", i + 1, ret, wcs[i].wr_id, ibv_wc_status_str(wcs[i].status));
      }
    return std::make_tuple(wcs, ret);
  }

  void VerbsConnection::notify_events(bool only_solicited)
  {
    impl::expect_zero(ibv_req_notify_cq(_qp->recv_cq, only_solicited));
  }

  int LibfabricConnection::wait_events(int timeout)
  {
    return fi_cntr_wait(_write_counter, _counter+1, timeout);
  }

  ibv_cq* VerbsConnection::wait_events()
  {
    ibv_cq* ev_cq = nullptr;
    void* ev_ctx = nullptr;
    impl::expect_zero(ibv_get_cq_event(_channel, &ev_cq, &ev_ctx));
    return ev_cq;
  }

  void VerbsConnection::ack_events(ibv_cq* cq, int len)
  {
    ibv_ack_cq_events(cq, len);
  }
}
