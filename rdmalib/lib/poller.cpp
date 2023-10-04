
#include <rdmalib/poller.hpp>

namespace rdmalib {

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

}
