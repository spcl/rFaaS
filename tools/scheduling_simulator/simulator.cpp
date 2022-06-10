
#include <mpi.h>

#include "simulator.hpp"


int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  int world_size, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  auto opts = simulator::opts(argc, argv);
  bool am_client = rank < opts.clients;

  if(am_client) {

    simulator::Client client{opts.seed, opts.cores_to_allocate, MPI_COMM_WORLD};
    client.initialize_seeds(opts.experiments);

    for(int i = 0; i < opts.experiments; ++i) {

      client.shuffle_executors(opts.clients, world_size, i);

      for(int j = 0; j < opts.repetitions; ++j) {
        client.allocate();
      }

    }

  } else {

    simulator::Executor exec{opts.cores_executor, MPI_COMM_WORLD};

    for(int i = 0; i < opts.experiments; ++i) {

      for(int j = 0; j < opts.repetitions; ++j) {
        exec.handle_requests();
      }

    }
  }

  MPI_Finalize();
  return 0;
}

