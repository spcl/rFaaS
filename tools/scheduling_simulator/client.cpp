
#include <algorithm>

#include "simulator.hpp"

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
    for(int exec : _executors) {
      allocation.set_allocation(_cores_to_allocate);
      allocation.send_allocation(exec);

      reply.recv_reply(exec);
      if(reply.succeeded()) {
        success = true;
        break;
      }

      repetitions += 1;
    }

    MPI_Barrier(_comm);
  }

}
