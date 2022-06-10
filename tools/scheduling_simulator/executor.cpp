
#include "simulator.hpp"

namespace simulator {

  void Executor::handle_requests()
  {
    MPI_Request barrier_request;
    MPI_Request msg_request;
    RequestMessage allocation{_comm};
    ReplyMessage reply{_comm};

    int idx;
    MPI_Status status;

    int free_cores = _cores;

    MPI_Ibarrier(_comm, &barrier_request);

    while(true) {

      allocation.recv_allocation(&msg_request);
      MPI_Request requests[] = {barrier_request, msg_request};
      MPI_Waitany(2, requests, &idx, &status);

      if(idx == 0) {
        MPI_Request_free(&msg_request);
        break;
      } else {

        // reply to the client
        int request = allocation.get_allocation();
        if(request <= free_cores) {
          reply.set_cores(request);
          free_cores -= request;
        } else if(free_cores > 0) {
          reply.set_cores(free_cores);
          free_cores = 0;
        } else {
          reply.set_failure();
        }
        reply.send_reply(status.MPI_SOURCE);

      }
    }
  }

}

