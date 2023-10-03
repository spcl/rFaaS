
#ifndef __RDMALIB_COMPLETION_POLLER_HPP__
#define __RDMALIB_COMPLETION_POLLER_HPP__

#include <fcntl.h>
#include <iostream>
#include <ostream>
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

    void notify_events(bool only_solicited)
    {
      impl::expect_zero(ibv_req_notify_cq(_recv_cq, only_solicited));
    }

    void set_nonblocking()
    {
      int flags = fcntl(_recv_cq->channel->fd, F_GETFL);
      impl::expect_nonnegative(fcntl(_recv_cq->channel->fd, F_SETFL, flags | O_NONBLOCK));
    }

    ibv_cq* wait_events()
    {
      ibv_cq* ev_cq = nullptr;
      void* ev_ctx = nullptr;
      //impl::expect_zero(ibv_get_cq_event(_recv_cq->channel, &ev_cq, &ev_ctx));
      ibv_get_cq_event(_recv_cq->channel, &ev_cq, &ev_ctx);
      return ev_cq;
    }

    void ack_events(ibv_cq* cq, int len)
    {
      ibv_ack_cq_events(cq, len);
    }

    ibv_comp_channel* channel() const
    {
      return _recv_cq->channel;
    }

  private:
    static const int _wc_size = 64;
    std::array<ibv_wc, _wc_size> _rcv_work_completions;
    ibv_cq* _recv_cq;
  };


  struct EventPoller {

    EventPoller()
    {
      _epoll_fd = epoll_create1(0);
      impl::expect_nonnegative(_epoll_fd);
    }

    bool add_channel(Poller & poller, uint32_t data)
    {

      int flags = fcntl(poller.channel()->fd, F_GETFL);
      int rc = fcntl(poller.channel()->fd, F_SETFL, flags | O_NONBLOCK);
      if (rc < 0) {
        spdlog::error(
          "Failed to change file descriptor of a channel, fd: {}",
          poller.channel()->fd
        );
        return false;
      }

      epoll_event ev;
      ev.events = EPOLLIN | EPOLLOUT;
      ev.data.u32 = data;

      int ret = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, poller.channel()->fd, &ev);
      if (ret == -1) {
        spdlog::error("Failed to add a file descriptor of completion event "
                      "channel to epoll, fd: {}",
                      poller.channel()->fd);
        return false;
      }

      return true;
    }

    std::tuple<epoll_event*, int> poll(int timeout_ms)
    {
      int events = epoll_wait(_epoll_fd, _events.data(), MAX_EVENTS, timeout_ms);
      impl::expect_nonnegative(events, true, "Failed to poll events with epoll!");

      return std::make_tuple(_events.data(), events);
    }

  private:
    int _epoll_fd;

    static constexpr int MAX_EVENTS = 8;
    std::array<epoll_event, MAX_EVENTS> _events;
  };

} // namespace rdmalib

#endif
