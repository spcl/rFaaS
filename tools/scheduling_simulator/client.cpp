
#include "simulator.hpp"
#include "executors.hpp"
#include "mpi_log.hpp"

namespace simulator {

  void Client::allocate(const Executors & executors)
  {
    RequestMessage allocation{_comm};
    ReplyMessage reply{_comm};

    bool success = false;
    int repetitions = 1;
    int cores_to_allocate = _cores_to_allocate;

    _results.start_iteration();

    for(int exec : executors.executors()) {

      _logger.debug("Sending allocation {} to executor {}.", cores_to_allocate, exec);
      allocation.set_allocation(cores_to_allocate);
      allocation.send_allocation(exec);

      reply.recv_reply(exec);
      if(reply.succeeded()) {

        _logger.debug("Allocated {} on executor {}.", reply.get_cores(), exec);
        cores_to_allocate -= reply.get_cores();

        if(cores_to_allocate <= 0) {

          _logger.debug("Succesfull allocation.");
          success = true;
          break;

        } else {
          _results.partial_allocation();
        }

      } else
        _results.failed_allocation();

      repetitions += 1;
    }
    _results.complete_allocation(success);

    MPI_Request request;
    MPI_Ibarrier(_comm, &request);
    MPI_Wait(&request, MPI_STATUS_IGNORE);
  }

}
