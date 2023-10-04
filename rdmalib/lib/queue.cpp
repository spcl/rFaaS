
#include <chrono>
#include <thread>

#ifndef USE_LIBFABRIC
#include <infiniband/verbs.h>
#else
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#endif
#include <spdlog/spdlog.h>

#include <rdmalib/queue.hpp>

namespace rdmalib {

  void RecvWorkCompletions::initialize_batched_recv(const rdmalib::impl::Buffer & buf, size_t offset)
  {
    for(int i = 0; i < _rbatch; i++){
      _rwc_sges[i] = buf.sge(offset, i*offset);

#ifndef USE_LIBFABRIC
      _batch_wrs[i].sg_list = _rwc_sges[i].array();
      _batch_wrs[i].num_sge = _rwc_sges[i].size();
#endif
    }
  }

#ifndef USE_LIBFABRIC
  std::tuple<ibv_wc*, int> RecvWorkCompletions::_poll(bool blocking, int count)
  {
    int ret = 0;
    ibv_wc* wcs = _rcv_work_completions.data();

    do {
      ret = ibv_poll_cq(
        _queue_pair->recv_cq,
        count == -1 ? _wc_size : count,
        wcs
      );
    } while(blocking && ret == 0);

    if(ret < 0) {
      spdlog::error("Failure of polling events from: {} queue! Return value {}, errno {}", "recv", ret, errno);
      return std::make_tuple(nullptr, -1);
    }
    if(ret)
      for(int i = 0; i < ret; ++i) {
        if(wcs[i].status != IBV_WC_SUCCESS) {
          spdlog::error(
            "Queue {} Work Completion {}/{} finished with an error {}, {}",
            "recv",
            i+1, ret, wcs[i].status, ibv_wc_status_str(wcs[i].status)
          );
        }
        SPDLOG_DEBUG("Queue {} Ret {}/{} WC {} Status {}", "recv", i + 1, ret, wcs[i].wr_id, ibv_wc_status_str(wcs[i].status));
      }
    return std::make_tuple(wcs, ret);
  }
#else
  std::tuple<fi_cq_data_entry *, int> RecvWorkCompletions::_poll(bool blocking, int count, bool update)
  {
    int ret = 0;
    fi_cq_data_entry *wcs = _rcv_work_completions.data();

    // spdlog::error("{} {} {}", fmt::ptr(_qp), fmt::ptr(_qp->recv_cq),
    // fmt::ptr(wcs));
    do {
      ret = fi_cq_read(_recv_cq, wcs, count == -1 ? _wc_size : count);
      if (ret == -FI_EAVAIL) {
        fi_cq_err_entry _ewc;
        ret = fi_cq_readerr(_recv_cq, &_ewc, 0);
        if (ret != 1)
          ret = -1;
        else
          spdlog::error("Queue {} connection {} WC {} finished with an error {}",
                        "recv", reinterpret_cast<uint64_t>(_ewc.op_context),
                        fi_strerror(_ewc.err));
      }
    } while (blocking && (ret == -EAGAIN || ret == 0));

    if (ret < 0 && ret != -EAGAIN) {
      spdlog::error("Ffi_cq_data_entryailure of polling events from: {} queue "
                    "connection {}! Return value {} message {} errno {}",
                    "recv", fmt::ptr(this), ret, fi_strerror(std::abs(ret)),
                    errno);
      return std::make_tuple(nullptr, -1);
    }
    if (ret > 0) {
      if (update)
        _counter += ret;
      for (int i = 0; i < ret; ++i) {
        SPDLOG_DEBUG("Connection {} Queue {} Ret {}/{} WC {}", fmt::ptr(this),
                    "recv", i + 1, ret,
                    reinterpret_cast<uint64_t>(wcs[i].op_context));
      }
    }
    return std::make_tuple(wcs, ret == -EAGAIN ? 0 : ret);
  }
#endif

  int32_t RecvWorkCompletions::post_batched_empty_recv(int count)
  {
#ifdef USE_LIBFABRIC
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
        ret = fi_recv(_queue_pair, begin.array()->iov_base, begin.array()->iov_len, begin.lkeys()[0], NULL, reinterpret_cast<void *>(j));
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
        ret = fi_recv(_queue_pair, begin.array()->iov_base, begin.array()->iov_len, begin.lkeys()[0], NULL, reinterpret_cast<void *>(j));
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
#else
    struct ibv_recv_wr* bad = nullptr;
    int loops = count / _rbatch;
    int reminder = count % _rbatch;
    SPDLOG_DEBUG("Batch {} {} to local QPN {}", loops, reminder, _queue_pair->qp_num);

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
      ret = ibv_post_recv(_queue_pair, &_batch_wrs[0], &bad);
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
      ret = ibv_post_recv(_queue_pair, _batch_wrs, &bad);
      _batch_wrs[reminder-1].next= &(_batch_wrs[reminder]);
    }

    if(ret) {
      spdlog::error("Batched Post empty recv  unsuccesful, reason {} {}", ret, strerror(ret));
      return -1;
    }

    SPDLOG_DEBUG("Batched Post empty recv succesfull");
    return count;
#endif
  }

#ifndef USE_LIBFABRIC
  RecvWorkCompletions::RecvWorkCompletions(int rcv_buf_size, ibv_qp* queue_pair):
#else
  RecvWorkCompletions::RecvWorkCompletions(int rcv_buf_size, fid_ep* queue_pair):
#endif
    _queue_pair(queue_pair),
    _rcv_buf_size(rcv_buf_size),
    _refill_threshold(std::min(_rcv_buf_size, DEFAULT_REFILL_THRESHOLD)),
    _requests(0)
  {

#ifndef USE_LIBFABRIC
    for(int i=0; i < _rbatch; i++){
      _batch_wrs[i].wr_id = i;
      _batch_wrs[i].sg_list = 0;
      _batch_wrs[i].num_sge = 0;
      _batch_wrs[i].next=&(_batch_wrs[i+1]);
    }
    _batch_wrs[_rbatch-1].next = NULL;
#else
    _counter = 0;
#endif

    if(this->_queue_pair) {
      refill();
    }

  }

#ifndef USE_LIBFABRIC
  std::tuple<ibv_wc*,int> RecvWorkCompletions::poll(bool blocking)
#else
  std::tuple<fi_cq_data_entry*,int> RecvWorkCompletions::poll(bool blocking, bool update)
#endif
  {
    auto wc = this->_poll(blocking, -1, update);
    if(std::get<1>(wc)) {
      SPDLOG_DEBUG("Polled reqs {}, left {}", std::get<1>(wc), _requests);
    }
    _requests -= std::get<1>(wc);
    return wc;
  }

  void RecvWorkCompletions::update_requests(int change)
  {
    _requests += change;
  }

  bool RecvWorkCompletions::refill()
  {
    if(_requests < _refill_threshold) {
      SPDLOG_DEBUG("Post {} requests to buffer at QP {}", _rcv_buf_size - _requests, fmt::ptr(this->qp()));
      this->post_batched_empty_recv(_rcv_buf_size - _requests);
      _requests = _rcv_buf_size;
      return true;
    }
    return false;
  }
}
