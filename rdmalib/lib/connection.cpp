
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
  {}

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
    wr.send_flags = IBV_SEND_SIGNALED;

    int ret = ibv_post_send(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post send unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    spdlog::debug("Post send succesfull");
    return _req_count - 1;
  }

  int32_t Connection::post_recv(ScatterGatherElement && elem, int32_t id)
  {
    // FIXME: extend with multiple sges
    struct ibv_recv_wr wr, *bad;
    wr.wr_id = id == -1 ? _req_count++ : id;
    wr.next = nullptr;
    wr.sg_list = elem.array();
    wr.num_sge = elem.size();

    int ret = ibv_post_recv(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post receive unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    spdlog::debug("Post recv succesfull");
    return _req_count - 1;
  }

  int32_t Connection::_post_write(ScatterGatherElement && elems, ibv_send_wr wr)
  {
    ibv_send_wr* bad;
    wr.wr_id = _req_count++;
    wr.next = nullptr;
    wr.sg_list = elems.array();
    wr.num_sge = elems.size();
    wr.send_flags = IBV_SEND_SIGNALED;

    int ret = ibv_post_send(_qp, &wr, &bad);
    if(ret) {
      spdlog::error("Post write unsuccesful, reason {} {}", errno, strerror(errno));
      return -1;
    }
    spdlog::debug("Post write succesfull, remote addr {}, remote rkey {}", wr.wr.rdma.remote_addr, wr.wr.rdma.rkey);
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
    spdlog::debug("Post write succesfull");
    return _req_count - 1;
  }

  std::optional<ibv_wc> Connection::poll_wc(QueueType type, bool blocking)
  {
    constexpr int entries = 1;
    ibv_wc wc;
    int ret = 0;
    if(blocking) {
      do {
        ret = ibv_poll_cq(type == QueueType::RECV ? _qp->recv_cq : _qp->send_cq, entries, &wc);
      } while(ret == 0);
    }
    else
      ret = ibv_poll_cq(type == QueueType::RECV ? _qp->recv_cq : _qp->send_cq, entries, &wc);
    if(ret)
      spdlog::debug("Received WC {} Status {}", wc.wr_id, ibv_wc_status_str(wc.status));
    return ret == 0 ? std::optional<ibv_wc>{} : wc;
  }

}
