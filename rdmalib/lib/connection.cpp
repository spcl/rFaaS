
#include <chrono>
#include <spdlog/spdlog.h>

#include <rdmalib/connection.hpp>
#include <thread>

namespace rdmalib {

  ConnectionConfiguration::ConnectionConfiguration()
  {
    memset(&attr, 0, sizeof(attr));
    memset(&conn_param, 0 , sizeof(conn_param));
  }

  ScatterGatherElement::ScatterGatherElement()
  {
  }

  ibv_sge * ScatterGatherElement::array()
  {
    return _sges.data();
  }

  size_t ScatterGatherElement::size()
  {
    return _sges.size();
  }

  Connection::Connection():
    _id(nullptr),
    _qp(nullptr),
    _req_count(0)
  {
    inlining(false);
  }

  Connection::~Connection()
  {
    close();
  }
 
  Connection::Connection(Connection&& obj):
    _id(obj._id),
    _qp(obj._qp),
    _req_count(obj._req_count)
  {
    obj._id = nullptr;
    obj._qp = nullptr;
    obj._req_count = 0;
  }

  void Connection::inlining(bool enable)
  {
    if(enable)
      _send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
    else
      _send_flags = IBV_SEND_SIGNALED;
  }

  void Connection::close()
  {
    if(_id) {
      rdma_destroy_qp(_id);
      rdma_destroy_id(_id);
      rdma_destroy_ep(_id);
      _id = nullptr;
    }
  }

  ibv_qp* Connection::qp() const
  {
    return this->_qp;
  }

  int32_t Connection::post_send(ScatterGatherElement && elems, int32_t id)
  {
    // FIXME: extend with multiple sges
    struct ibv_send_wr wr, *bad;
    wr.wr_id = id == -1 ? _req_count++ : id;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = _send_flags;

    int ret = ibv_post_send(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post send unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    SPDLOG_DEBUG("Post send succesfull");
    return _req_count - 1;
  }

  int32_t Connection::post_recv(ScatterGatherElement && elem, int32_t id, int count)
  {
    // FIXME: extend with multiple sges
    struct ibv_recv_wr wr, *bad;
    wr.wr_id = id == -1 ? _req_count++ : id;
    wr.next = nullptr;
    wr.sg_list = elem.array();
    wr.num_sge = elem.size();

    int ret;
    for(int i = 0; i < count; ++i) {
      ret = ibv_post_recv(_qp, &wr, &bad);
      if(ret)
        break;
    }
    if(ret) {
      spdlog::error("Post receive unsuccesful, reason {} {}", ret, strerror(ret));
      return -1;
    }
    SPDLOG_DEBUG("Post recv succesfull");
    return wr.wr_id;
  }

  int32_t Connection::_post_write(ScatterGatherElement && elems, ibv_send_wr wr)
  {
    ibv_send_wr* bad;
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.send_flags = _send_flags;

    int ret = ibv_post_send(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post write unsuccesful, reason {} {}", ret, strerror(ret));
      return -1;
    }
    SPDLOG_DEBUG(
        "Post write succesfull id: {}, sge size: {}, first lkey {} len {}, remote addr {}, remote rkey {}, imm data {}",
        wr.wr_id, wr.num_sge, wr.sg_list[0].lkey, wr.sg_list[0].length, wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, ntohl(wr.imm_data)
    );
    return _req_count - 1;

  }

  int32_t Connection::post_write(ScatterGatherElement && elems, const RemoteBuffer & rbuf)
  {
    ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.wr.rdma.remote_addr = rbuf.addr;
    wr.wr.rdma.rkey = rbuf.rkey;
    return _post_write(std::forward<ScatterGatherElement>(elems), wr);
  }

  int32_t Connection::post_write(ScatterGatherElement && elems, const RemoteBuffer & rbuf, uint32_t immediate)
  {
    ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.imm_data = htonl(immediate);
    wr.wr.rdma.remote_addr = rbuf.addr;
    wr.wr.rdma.rkey = rbuf.rkey;
    return _post_write(std::forward<ScatterGatherElement>(elems), wr);
  }

  int32_t Connection::post_cas(ScatterGatherElement && elems, const RemoteBuffer & rbuf, uint64_t compare, uint64_t swap)
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

  std::tuple<ibv_wc*, int> Connection::poll_wc(QueueType type, bool blocking)
  {
    memset(_wc.data(), 0, sizeof(ibv_wc) * _wc.size());
    int ret = 0;
    if(blocking) {
      do {
        ret = ibv_poll_cq(type == QueueType::RECV ? _qp->recv_cq : _qp->send_cq, _wc.size(), _wc.data());
      } while(ret == 0);
    }
    else
      ret = ibv_poll_cq(type == QueueType::RECV ? _qp->recv_cq : _qp->send_cq, _wc.size(), _wc.data());
    if(ret < 0) {
      spdlog::error("Failure of polling events from: {} queue! Return value {}, errno {}", type == QueueType::RECV ? "recv" : "send", ret, errno);
      return std::make_tuple(nullptr, -1);
    }
    if(ret)
      for(int i = 0; i < ret; ++i)
        SPDLOG_DEBUG("Queue {} Ret {}/{} WC {} Status {}", type == QueueType::RECV ? "recv" : "send", i + 1, ret, _wc[i].wr_id, ibv_wc_status_str(_wc[i].status));
    return std::make_tuple(_wc.data(), ret);
  }

}
