
#include <mpi.h>

#include <spdlog/spdlog.h>

#include "simulator.hpp"
#include "mpi_log.hpp"


int main(int argc, char** argv)
{
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::set_level(spdlog::level::debug);
  MPI_Init(&argc, &argv);

  int world_size, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  simulator::log::Logger logger{rank};

  auto opts = simulator::opts(argc, argv);
  bool am_client = rank < opts.clients;

  if(am_client) {

    logger.info("Executing simulator, client role.", world_size);

    simulator::Client client{opts.seed + rank, opts.cores_to_allocate, MPI_COMM_WORLD, logger};
    client.initialize_seeds(opts.experiments);

    for(int i = 0; i < opts.experiments; ++i) {

      client.shuffle_executors(opts.clients, world_size, i);

      for(int j = 0; j < opts.repetitions; ++j) {
        logger.debug("Begin repetition {} of experiment {}.", j, i);
        MPI_Barrier(MPI_COMM_WORLD);
        client.allocate();
      }

    }

  } else {

    logger.info("Executing simulator, server role.", world_size);
    simulator::Executor exec{opts.cores_executor, MPI_COMM_WORLD, logger};

    for(int i = 0; i < opts.experiments; ++i) {

      for(int j = 0; j < opts.repetitions; ++j) {
        logger.debug("Begin repetition {} of experiment {}.", j, i);
        MPI_Barrier(MPI_COMM_WORLD);
        exec.handle_requests();
      }

    }
  }

  MPI_Finalize();
  return 0;
}

