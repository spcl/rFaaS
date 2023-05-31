
#include <iterator>
#include <tuple>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <spdlog/spdlog.h>

#include <rdmalib/allocation.hpp>
#include <rdmalib/rdmalib.hpp>

#include "executor_process.hpp"
#include "settings.hpp"
#include "../common.hpp"

namespace rfaas::executor_manager {

  ActiveExecutor::~ActiveExecutor()
  {
    for(int i = 0; i < connections_len; ++i) {
      delete connections[i];
    }
    delete[] connections; 
  }

  ProcessExecutor::ProcessExecutor(int cores, ProcessExecutor::time_t alloc_begin, pid_t pid):
    ActiveExecutor(cores),
    _pid(pid)
  {
    _allocation_begin = alloc_begin;
    // FIXME: remove after connection
    _allocation_finished = _allocation_begin;
  }

  std::tuple<ProcessExecutor::Status,int> ProcessExecutor::check() const
  {
    int status;
    pid_t return_pid = waitpid(_pid, &status, WNOHANG);
    if(!return_pid) {
      return std::make_tuple(Status::RUNNING, 0);
    } else {
      if(WIFEXITED(status)) {
        return std::make_tuple(Status::FINISHED, WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
        return std::make_tuple(Status::FINISHED_FAIL, WTERMSIG(status));
      } else {
        // Unknown problem
        return std::make_tuple(Status::FINISHED_FAIL, -1);
      }
    }
  }

  int ProcessExecutor::id() const
  {
    return static_cast<int>(_pid);
  }

  ProcessExecutor* ProcessExecutor::spawn(
    const rdmalib::AllocationRequest & request,
    const ExecutorSettings & exec,
    const executor::ManagerConnection & conn
  )
  {
    static int counter = 0;
    auto begin = std::chrono::high_resolution_clock::now();
    //spdlog::info("Child fork begins work on PID {} req {}", mypid, fmt::ptr(&request));
    std::string client_addr{request.listen_address};
    std::string client_port = std::to_string(request.listen_port);
    //spdlog::error("Child fork begins work on PID {} req {}", mypid, fmt::ptr(&request));
    std::string client_in_size = std::to_string(request.input_buf_size);
    std::string client_func_size = std::to_string(request.func_buf_size);
    std::string client_cores = std::to_string(request.cores);
    std::string client_timeout = std::to_string(request.hot_timeout);
    //spdlog::error("Child fork begins work on PID {}", mypid);
    std::string executor_repetitions = std::to_string(exec.repetitions);
    std::string executor_warmups = std::to_string(exec.warmup_iters);
    std::string executor_recv_buf = std::to_string(exec.recv_buffer_size);
    std::string executor_max_inline = std::to_string(exec.max_inline_data);
    std::string executor_pin_threads;
    if(exec.pin_threads >= 0)
      executor_pin_threads = std::to_string(0);//counter++);
    else
      executor_pin_threads = std::to_string(exec.pin_threads);
      auto sandbox_type = exec.sandbox_type;

    std::string mgr_port = std::to_string(conn.port);
    std::string mgr_secret = std::to_string(conn.secret);
    std::string mgr_buf_addr = std::to_string(conn.r_addr);
    std::string mgr_buf_rkey = std::to_string(conn.r_key);

    int mypid = vfork();
    if(mypid < 0) {
      spdlog::error("Fork failed! {}", mypid);
    }
    if(mypid == 0) {
      mypid = getpid();
      auto out_file = ("executor_" + std::to_string(mypid));

      spdlog::info("Child fork begins work on PID {}, using sandbox {}", mypid, sandbox_serialize(sandbox_type));
      int fd = open(out_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      dup2(fd, 1);
      dup2(fd, 2);

      std::vector<std::string> argv;
      std::vector<const char*> additional_args;
      if(sandbox_type == SandboxType::PROCESS) {
        argv = {
          "executor",
          "-a", client_addr.c_str(),
          "-p", client_port.c_str(),
          "--polling-mgr", "thread",
          "-r", executor_repetitions.c_str(),
          "-x", executor_recv_buf.c_str(),
          "-s", client_in_size.c_str(),
          "--pin-threads", executor_pin_threads.c_str(),
          "--fast", client_cores.c_str(),
          "--warmup-iters", executor_warmups.c_str(),
          "--max-inline-data", executor_max_inline.c_str(),
          "--func-size", client_func_size.c_str(),
          "--timeout", client_timeout.c_str(),
          "--mgr-address", conn.addr.c_str(),
          "--mgr-port", mgr_port.c_str(),
          "--mgr-secret", mgr_secret.c_str(),
          "--mgr-buf-addr", mgr_buf_addr.c_str(),
          "--mgr-buf-rkey", mgr_buf_rkey.c_str(),
          nullptr
        };
      } else if (sandbox_type == SandboxType::SARUS) {
        argv = {
          "sarus", "run"
        };

        exec.sandbox_config->generate_args(argv, exec.sandbox_user);
        argv.emplace_back(exec.sandbox_name);
        argv.emplace_back(exec.sandbox_config->get_executor_path());

        additional_args = {
          "-a", client_addr.c_str(),
          "-p", client_port.c_str(),
          "--polling-mgr", "thread",
          "-r", executor_repetitions.c_str(),
          "-x", executor_recv_buf.c_str(),
          "-s", client_in_size.c_str(),
          "--pin-threads", executor_pin_threads.c_str(),
          "--fast", client_cores.c_str(),
          "--warmup-iters", executor_warmups.c_str(),
          "--max-inline-data", executor_max_inline.c_str(),
          "--func-size", client_func_size.c_str(),
          "--timeout", client_timeout.c_str(),
          "--mgr-address", conn.addr.c_str(),
          "--mgr-port", mgr_port.c_str(),
          "--mgr-secret", mgr_secret.c_str(),
          "--mgr-buf-addr", mgr_buf_addr.c_str(),
          "--mgr-buf-rkey", mgr_buf_rkey.c_str(),
          nullptr
        };
      } else if (sandbox_type == SandboxType::DOCKER) {
          argv = {
            "docker_rdma_sriov", "run", "--rm", "-i"
        };
        DockerConfiguration config = std::get<DockerConfiguration>(*exec.sandbox_config);
        config.generate_args(argv);

        additional_args = {
          "/opt/bin/executor",
          "-a", client_addr.c_str(),
          "-p", client_port.c_str(),
          "--polling-mgr", "thread",
          "-r", executor_repetitions.c_str(),
          "-x", executor_recv_buf.c_str(),
          "-s", client_in_size.c_str(),
          "--pin-threads", executor_pin_threads.c_str(),
          "--fast", client_cores.c_str(),
          "--warmup-iters", executor_warmups.c_str(),
          "--max-inline-data", executor_max_inline.c_str(),
          "--func-size", client_func_size.c_str(),
          "--timeout", client_timeout.c_str(),
          "--mgr-address", conn.addr.c_str(),
          "--mgr-port", mgr_port.c_str(),
          "--mgr-secret", mgr_secret.c_str(),
          "--mgr-buf-addr", mgr_buf_addr.c_str(),
          "--mgr-buf-rkey", mgr_buf_rkey.c_str(),
          nullptr
        };
      } else if(sandbox_type == SandboxType::SINGULARITY) {
        // Handle Singularity case
        SingularityConfiguration config = std::get<SingularityConfiguration>(*exec.sandbox_config);
      }

      std::vector<const char*> cstrings_argv;
      std::transform(argv.begin(), argv.end(), std::back_inserter(cstrings_argv),
        [](const std::string & input) -> const char* {
          return input.c_str();
        }
      );
      std::copy(additional_args.begin(), additional_args.end(), std::back_inserter(cstrings_argv));

      SPDLOG_DEBUG("Executor launch arguments");
      for(const char* str : cstrings_argv)
        if(str)
            std::cout<<str<<std::endl;
          //SPDLOG_DEBUG(str);

      int ret = execvp(cstrings_argv.data()[0], const_cast<char**>(&cstrings_argv.data()[0]));
      if(ret == -1) {
        spdlog::error("Executor process failed {}, reason {}", errno, strerror(errno));
        close(fd);
        exit(1);
      }

      //close(fd);
      exit(0);
    }
    if(counter == 36)
      counter = 0;
    return new ProcessExecutor{request.cores, begin, mypid};
  }

}

