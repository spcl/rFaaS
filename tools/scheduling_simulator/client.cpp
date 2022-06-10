
#include <algorithm>

#include "simulator.hpp"
#include "mpi_log.hpp"

namespace simulator {

  void Client::initialize_seeds(int iterations)
  {
    _random_seeds.resize(iterations);
    std::generate_n(_random_seeds.begin(), iterations, _prng);
  }

  void Client::shuffle_executors(int low, int high, int iteration)
  {
    _executors.resize(high - low + 1);
    std::iota(_executors.begin(), _executors.end(), low);

    // shuffle
    _prng.seed(_random_seeds[iteration]);
    std::shuffle(_executors.begin(), _executors.end(), _prng);
  }

  void Client::allocate()
  {
    RequestMessage allocation{_comm};
    ReplyMessage reply{_comm};

    bool success = false;
    int repetitions = 1;
    int cores_to_allocate = _cores_to_allocate;

    for(int exec : _executors) {

      _logger.debug("Sending allocation {} to executor {}.", cores_to_allocate, exec);
      allocation.set_allocation(cores_to_allocate);
      allocation.send_allocation(exec);

      reply.recv_reply(exec);
      if(reply.succeeded()) {
        _logger.debug("Allocated {} on executor {}.", reply.get_cores(), exec);
        cores_to_allocate -= reply.get_cores();
      }

      if(cores_to_allocate <= 0) {
        _logger.debug("Succesfull allocation.");
        success = true;
        break;
      }

      repetitions += 1;
    }

    MPI_Request request;
    MPI_Ibarrier(_comm, &request);
    MPI_Wait(&request, MPI_STATUS_IGNORE);
  }

}
