
#ifndef __RDMALIB_COMPLETION_POLLER_HPP__
#define __RDMALIB_COMPLETION_POLLER_HPP__

#include <fcntl.h>
#include <sys/epoll.h>

#include <infiniband/verbs.h>
#include <spdlog/spdlog.h>

#include <rdmalib/util.hpp>

namespace rdmalib {

  struct Poller
  {
    Poller(ibv_cq* recv_cq = nullptr):
      _recv_cq(recv_cq)
    {}

    void initialize(ibv_cq* recv_cq)
    {
      spdlog::error("POLLER {}", fmt::ptr(recv_cq));
      _recv_cq = recv_cq;
    }

    bool initialized() const
    {
      return _recv_cq;
    }

    std::tuple<ibv_wc*, int> poll(bool blocking = false, int count = -1);

  private:
    static const int _wc_size = 32;
    std::array<ibv_wc, _wc_size> _rcv_work_completions;
    ibv_cq* _recv_cq;
  };

  //
  //  CompletionPoller() {
  //    _epoll_fd = epoll_create1(0);
  //    impl::expect_nonnegative(_epoll_fd);
  //
  //    _callbacks.resize(MAX_CALLBACKS);
  //  }
  //
  //  bool add_channel(ibv_comp_channel *cq, std::function<void(void)> &&callback) {
  //    int flags = fcntl(_completion_channel->fd, F_GETFL);
  //    int rc = fcntl(_completion_channel->fd, F_SETFL, flags | O_NONBLOCK);
  //    if (rc < 0) {
  //      spdlog::error(
  //          "Failed to change file descriptor of completion event channel fd: {}",
  //          _completion_channel->fd);
  //      return false;
  //    }
  //
  //    _callbacks.push_back(callback);
  //
  //    epoll_event ev;
  //    ev.events = EPOLLIN | EPOLLOUT;
  //    ev.data.ptr = reinterpret_cast<void *>(&_callbacks.back());
  //
  //    int ret = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _completion_channel->fd, &ev);
  //    if (ret == -1) {
  //      spdlog::error("Failed to add a file descriptor of completion event "
  //                    "channel to epoll, fd: {}",
  //                    _completion_channel->fd);
  //      _callbacks.erase(_callbacks) return false;
  //    }
  //
  //    return true;
  //  }
  //
  //private:
  //  int _epoll_fd;
  //  ibv_comp_channel *_completion_channel;
  //
  //  static constexpr int MAX_CALLBACKS = 1024;
  //  std::vector<std::function<void(void)>> _callbacks;
  //};

} // namespace rdmalib

#endif
