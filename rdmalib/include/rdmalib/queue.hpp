
#ifndef __RDMALIB_SHARED_QUEUE_HPP__
#define __RDMALIB_SHARED_QUEUE_HPP__

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <queue>
#include <spdlog/spdlog.h>
#include <vector>

#include <infiniband/verbs.h>

#include <rdma/rdma_cma.h>
#include <rdmalib/buffer.hpp>

namespace rdmalib {

  struct SendWorkCompletions {

    ibv_qp* qp() const
    {
      return _queue_pair;
    }

    void set_qp(ibv_qp* qp)
    {
      _queue_pair = qp;
    }

    ibv_wc* wcs()
    {
      return _send_work_completions.data();
    }

    size_t wc_size()
    {
      return _send_work_completions.size();
    }

    //std::tuple<ibv_wc*, int> poll(bool blocking = false, int count = -1)
    //{
    //  int ret = 0;
    //  ibv_wc* wcs = _send_work_completions.data();

    //  //spdlog::error("{} {} {}", fmt::ptr(_qp), fmt::ptr(_qp->recv_cq), fmt::ptr(wcs));
    //  do {
    //    ret = ibv_poll_cq(
    //      _queue_pair->send_cq,
    //      count == -1 ? _wc_size : count,
    //      wcs
    //    );
    //  } while(blocking && ret == 0);

    //  if(ret < 0) {
    //    spdlog::error("Failure of polling events from: {} queue! Return value {}, errno {}", "send", ret, errno);
    //    return std::make_tuple(nullptr, -1);
    //  }
    //  if(ret)
    //    for(int i = 0; i < ret; ++i) {
    //      if(wcs[i].status != IBV_WC_SUCCESS) {
    //        spdlog::error(
    //          "Queue send Work Completion {}/{} finished with an error {}, {}",
    //          i+1, ret, wcs[i].status, ibv_wc_status_str(wcs[i].status)
    //        );
    //      }
    //      SPDLOG_DEBUG("Queue send Ret {}/{} WC {} Status {}", i + 1, ret, wcs[i].wr_id, ibv_wc_status_str(wcs[i].status));
    //    }
    //  return std::make_tuple(wcs, ret);
    //}

    SendWorkCompletions(ibv_qp* queue_pair):
      _queue_pair(queue_pair)
    {}

  private:

    ibv_qp* _queue_pair;

    static const int _wc_size = 32;
    std::array<ibv_wc, _wc_size> _send_work_completions;
  };

  struct RecvWorkCompletions {

    ibv_qp* qp() const
    {
      return _queue_pair;
    }

    void set_qp(ibv_qp* qp)
    {
      _queue_pair = qp;
      //refill();
    }

    ibv_cq* receive_cq() const
    {
      return _queue_pair->recv_cq;
    }

    ibv_wc* wcs()
    {
      return _rcv_work_completions.data();
    }

    size_t wc_size()
    {
      return _rcv_work_completions.size();
    }

    int rcv_buf_size() const
    {
      return _rcv_buf_size;
    }

    void initialize_batched_recv(const rdmalib::impl::Buffer & buf, size_t offset);

    std::tuple<ibv_wc*, int> _poll(bool blocking = false, int count = -1);

    int32_t post_batched_empty_recv(int count);

    RecvWorkCompletions(int rcv_buf_size, ibv_qp* queue_pair = nullptr);

    template<typename T>
    inline void initialize(const rdmalib::Buffer<T>& receive_buffer, int stride = -1)
    {
      if(!_queue_pair) {
        return;
      }

      if(stride == -1) {
        this->initialize_batched_recv(receive_buffer, sizeof(typename rdmalib::Buffer<T>::value_type));
      } else {
        this->initialize_batched_recv(receive_buffer, stride);
      }

      _requests = 0;
      this->refill();
    }

    std::tuple<ibv_wc*,int> poll(bool blocking = false);

    void update_requests(int change);

    bool refill();

  private:

    ibv_qp* _queue_pair;

    int _rcv_buf_size;
    int _refill_threshold;
    int _requests;
    constexpr static int DEFAULT_REFILL_THRESHOLD = 8;

    static const int _rbatch = 32; // 32 for faster division in the code
    struct ibv_recv_wr _batch_wrs[_rbatch]; // preallocated and prefilled batched recv.

    static const int _wc_size = 32;
    std::array<ibv_wc, _wc_size> _rcv_work_completions;
    std::array<ScatterGatherElement, _wc_size> _rwc_sges;
  };

} // namespace rdmalib

#endif
