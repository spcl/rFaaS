
#include <rdmalib/poller.hpp>

namespace rdmalib {

#ifndef USE_LIBFABRIC
  std::tuple<ibv_wc*, int> Poller::poll(bool blocking, int count)
  {
    int ret = 0;
    ibv_wc* wcs = _rcv_work_completions.data();

    if(!_recv_cq) {
      return std::make_tuple(nullptr, 0);
    }

    do {
      ret = ibv_poll_cq(
        _recv_cq,
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
  std::tuple<fi_cq_data_entry *, int> Poller::poll(bool blocking, int count, bool update)
  {
    int ret = 0;
    fi_cq_data_entry* wcs = _rcv_work_completions.data();

    //spdlog::error("{} {} {}", fmt::ptr(_qp), fmt::ptr(_qp->recv_cq), fmt::ptr(wcs));
    do {
      ret = fi_cq_read(
        _recv_cq,
        wcs,
        count == -1 ? _wc_size : count
      );
      if (ret == -FI_EAVAIL) {
        fi_cq_err_entry _ewc;
        ret = fi_cq_readerr(_recv_cq, &_ewc, 0);
        if (ret != 1)
          ret = -1;
        else
          spdlog::error(
              "Queue {} connection {} WC {} finished with an error {}",
              "recv",
              reinterpret_cast<uint64_t>(_ewc.op_context),
              fi_strerror(_ewc.err)
            );
      }
    } while(blocking && (ret == -EAGAIN || ret == 0));

    if(ret < 0 && ret != -EAGAIN) {
      spdlog::error("Failure of polling events from: {} queue connection {}! Return value {} message {} errno {}", "recv", fmt::ptr(this), ret, fi_strerror(std::abs(ret)), errno);
      return std::make_tuple(nullptr, -1);
    }
    if(ret > 0) {
      if (update)
        _counter += ret;
      for(int i = 0; i < ret; ++i) {
        SPDLOG_DEBUG("Connection {} Queue {} Ret {}/{} WC {}", fmt::ptr(this), "recv", i + 1, ret, reinterpret_cast<uint64_t>(wcs[i].op_context));
      }
    }
    return std::make_tuple(wcs, ret == -EAGAIN ? 0 : ret);
  }
#endif

}
