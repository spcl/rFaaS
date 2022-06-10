
#include <fstream>
#include <filesystem>

#include <mpi.h>

#include <spdlog/spdlog.h>
#include <cereal/archives/json.hpp>

#include "simulator.hpp"
#include "executors.hpp"
#include "results.hpp"
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

  if(rank == 0) {
    std::filesystem::create_directory(std::filesystem::path{opts.output});
  }

  if(am_client) {

    logger.info("Executing simulator, client role.", world_size);

    simulator::ClientResults results;
    simulator::Client client{opts.cores_to_allocate, MPI_COMM_WORLD, results, logger};
    simulator::Executors executors{opts.seed + rank};
    executors.initialize_seeds(opts.experiments);

    for(int i = 0; i < opts.experiments; ++i) {

      executors.shuffle_executors(opts.clients, world_size, i);
      results.begin_experiment(executors);

      for(int j = 0; j < opts.repetitions; ++j) {
        logger.debug("Begin repetition {} of experiment {}.", j, i);
        MPI_Barrier(MPI_COMM_WORLD);
        client.allocate(executors);
      }

    }

    {
      std::ofstream out_file{std::filesystem::path{opts.output} / ("client_" + std::to_string(rank) + ".json")};
      cereal::JSONOutputArchive archive_out(out_file);
      results.save(archive_out);
    }

  } else {

    logger.info("Executing simulator, server role.", world_size);
    simulator::ExecutorResults results;
    simulator::Executor exec{opts.cores_executor, MPI_COMM_WORLD, results, logger};

    for(int i = 0; i < opts.experiments; ++i) {

      results.begin_experiment();

      for(int j = 0; j < opts.repetitions; ++j) {

        logger.debug("Begin repetition {} of experiment {}.", j, i);
        MPI_Barrier(MPI_COMM_WORLD);
        exec.handle_requests();

      }

    }

    {
      std::ofstream out_file{std::filesystem::path{opts.output} / ("server_" + std::to_string(rank - opts.clients) + ".json")};
      cereal::JSONOutputArchive archive_out(out_file);
      results.save(archive_out);
    }
  }

  MPI_Finalize();
  return 0;
}

