
#ifndef __RDMALIB_SHARED_QUEUE_HPP__
#define __RDMALIB_SHARED_QUEUE_HPP__

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <queue>
#include <spdlog/spdlog.h>
#include <vector>

#ifndef USE_LIBFABRIC
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#else
#include <rdma/fi_eq.h>
#endif

#include <rdmalib/buffer.hpp>

namespace rdmalib {

  struct SendWorkCompletions {

#ifndef USE_LIBFABRIC
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

    SendWorkCompletions(ibv_qp* queue_pair):
      _queue_pair(queue_pair)
    {}
#else
    fid_ep* qp() const
    {
      return _queue_pair;
    }

    void set_qp(fid_ep* qp)
    {
      _queue_pair = qp;
    }

    fi_cq_data_entry* wcs()
    {
      return _send_work_completions.data();
    }

    SendWorkCompletions(fid_ep* queue_pair):
      _queue_pair(queue_pair)
    {}
#endif
  
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

  private:

#ifndef USE_LIBFABRIC
    ibv_qp* _queue_pair;

    static const int _wc_size = 32;
    std::array<ibv_wc, _wc_size> _send_work_completions;
#else
    fid_ep* _queue_pair;

    static const int _wc_size = 32;
    std::array<fi_cq_data_entry, _wc_size> _send_work_completions; 
#endif
  };

  struct RecvWorkCompletions {

#ifndef USE_LIBFABRIC
    ibv_qp* qp() const
    {
      return _queue_pair;
    }

    void set_qp(ibv_qp* qp)
    {
      _queue_pair = qp;
    }

    ibv_cq* receive_cq() const
    {
      return _queue_pair->recv_cq;
    }

    ibv_wc* wcs()
    {
      return _send_work_completions.data();
    }

    RecvWorkCompletions(int rcv_buf_size, ibv_qp* queue_pair = nullptr);

    std::tuple<ibv_wc*, int> _poll(bool blocking = false, int count = -1);

    std::tuple<ibv_wc*,int> poll(bool blocking = false);
#else
    fid_ep* qp() const
    {
      return _queue_pair;
    }

    void set_qp(fid_ep* qp)
    {
      _queue_pair = qp;
    }

    void set_cq(fid_cq* cq)
    {
      _recv_cq = cq;
    }

    fid_cq* receive_cq() const
    {
      return _recv_cq;
    }

    fi_cq_data_entry* wcs()
    {
      return _rcv_work_completions.data();
    }

    RecvWorkCompletions(uint32_t conn_id, int rcv_buf_size, fid_ep* queue_pair = nullptr);

    std::tuple<fi_cq_data_entry*, int> _poll(bool blocking = false, int count = -1, bool update = false);

    std::tuple<fi_cq_data_entry*,int> poll(bool blocking = false, bool update = false);

#endif

    size_t wc_size()
    {
      return _rcv_work_completions.size();
    }

    int rcv_buf_size() const
    {
      return _rcv_buf_size;
    }

    void initialize_batched_recv(const rdmalib::impl::Buffer & buf, size_t offset);

    int32_t post_batched_empty_recv(int count);

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

    void update_requests(int change);

    bool refill();

  private:

#ifndef USE_LIBFABRIC
    ibv_qp* _queue_pair;
#else
    fid_ep* _queue_pair;
    fid_cq* _recv_cq;
    int _counter;
#endif

    uint32_t _conn_id;
    int _rcv_buf_size;
    int _refill_threshold;
    int _requests;
    constexpr static int DEFAULT_REFILL_THRESHOLD = 8;

    static const int _wc_size = 32;
    static const int _rbatch = 32; // 32 for faster division in the code
#ifndef USE_LIBFABRIC
    struct ibv_recv_wr _batch_wrs[_rbatch]; // preallocated and prefilled batched recv.
    std::array<ibv_wc, _wc_size> _rcv_work_completions;
#else
    std::array<fi_cq_data_entry, _wc_size> _rcv_work_completions;
#endif

    std::array<ScatterGatherElement, _wc_size> _rwc_sges;
  };

} // namespace rdmalib

#endif
