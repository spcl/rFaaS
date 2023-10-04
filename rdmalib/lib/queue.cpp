
#include <chrono>
#include <thread>

#include <infiniband/verbs.h>
#include <spdlog/spdlog.h>

#include <rdmalib/queue.hpp>

namespace rdmalib {

  void RecvWorkCompletions::initialize_batched_recv(const rdmalib::impl::Buffer & buf, size_t offset)
  {
    for(int i = 0; i < _rbatch; i++){
      _rwc_sges[i] = buf.sge(offset, i*offset);
      _batch_wrs[i].sg_list = _rwc_sges[i].array();
      _batch_wrs[i].num_sge = _rwc_sges[i].size();
    }
  }

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

  int32_t RecvWorkCompletions::post_batched_empty_recv(int count)
  {
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
  }

  RecvWorkCompletions::RecvWorkCompletions(int rcv_buf_size, ibv_qp* queue_pair):
    _queue_pair(queue_pair),
    _rcv_buf_size(rcv_buf_size),
    _refill_threshold(std::min(_rcv_buf_size, DEFAULT_REFILL_THRESHOLD)),
    _requests(0)
  {

    for(int i=0; i < _rbatch; i++){
      _batch_wrs[i].wr_id = i;
      _batch_wrs[i].sg_list = 0;
      _batch_wrs[i].num_sge = 0;
      _batch_wrs[i].next=&(_batch_wrs[i+1]);
    }
    _batch_wrs[_rbatch-1].next = NULL;

    if(this->_queue_pair) {
      refill();
    }

  }

  std::tuple<ibv_wc*,int> RecvWorkCompletions::poll(bool blocking)
  {
    auto wc = this->_poll(blocking);
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
