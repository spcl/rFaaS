
#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/util.hpp>

#include <rfaas/allocation.hpp>
#include <rfaas/connection.hpp>
#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>

// FIXME: same function as in server/functions.cpp - merge?
#include <sys/mman.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <poll.h>

namespace rfaas {

  const polling_type polling_type::HOT_ALWAYS = polling_type{-1};
  const polling_type polling_type::WARM_ALWAYS = polling_type{0};

  polling_type::polling_type(int timeout):
    _timeout(timeout)
  {}

  polling_type::operator int() const
  {
    return _timeout;
  }

  executor_state::executor_state(rdmalib::Connection* conn, int rcv_buf_size):
    conn(conn)
  {
  }

  executor::executor(const std::string& address, int port, int numcores, int memory, int lease_id, device_data & dev):
    _state(dev.ip_address, dev.port, dev.default_receive_buffer_size + 1),
    _execs_buf(MAX_REMOTE_WORKERS),
    _device(dev),
    _numcores(numcores),
    _memory(memory),
    _executions(0),
    _invoc_id(0),
    _lease_id(lease_id)
  {
    _execs_buf.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    events = 0;
    _active_polling = false;
    _end_requested = false;

    // Enables sharing receive queue across all connections.
    _state.register_shared_queue(0, true);

    _exec_manager.reset(
      new manager_connection(
        address,
        port,
        dev.default_receive_buffer_size,
        dev.max_inline_data
      )
    );
  }

  executor::~executor()
  {
    this->deallocate();
  }

  executor::executor(executor&& obj):
    _state(std::move(obj._state)),
    _execs_buf(std::move(obj._execs_buf)),
    _device(std::move(obj._device)),
    _numcores(std::move(obj._numcores)),
    _memory(std::move(obj._memory)),
    _executions(std::move(obj._executions)),
    _invoc_id(std::move(obj._invoc_id)),
    _lease_id(std::move(obj._lease_id)),
    _connections(std::move(obj._connections)),
    _exec_manager(std::move(obj._exec_manager)),
    _func_names(std::move(obj._func_names)),
    _futures(std::move(obj._futures)),
    _background_thread(std::move(obj._background_thread))
  {
    _end_requested = obj._end_requested.load();
    obj._end_requested.store(false);

    _active_polling = obj._active_polling.load();
    obj._active_polling.store(false);
  }

  executor& executor::operator=(executor&& obj)
  {

    this->deallocate();

    _state = std::move(obj._state);
    _execs_buf = std::move(obj._execs_buf);
    _device = std::move(obj._device);
    _numcores = std::move(obj._numcores);
    _memory = std::move(obj._memory);
    _executions = std::move(obj._executions);
    _invoc_id = std::move(obj._invoc_id);
    _lease_id = std::move(obj._lease_id);
    _connections = std::move(obj._connections);
    _exec_manager = std::move(obj._exec_manager);
    _func_names = std::move(obj._func_names);
    _futures = std::move(obj._futures);
    _background_thread = std::move(obj._background_thread);

    _end_requested = obj._end_requested.load();
    obj._end_requested.store(false);

    _active_polling = obj._active_polling.load();
    obj._active_polling.store(false);


    return *this;
  }

  rdmalib::Buffer<char> executor::load_library(std::string path)
  {
    _func_names.clear();
    // Load the shared library with functions code
    FILE* file = fopen(path.c_str(), "rb");
    fseek (file, 0 , SEEK_END);
    size_t len = ftell(file);
    rewind(file);
    rdmalib::Buffer<char> functions(len);
    rdmalib::impl::expect_true(fread(functions.data(), 1, len, file) == len);
    functions.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
    fclose(file);

    // FIXME: same function as in server/functions.cpp - merge?
    // https://stackoverflow.com/questions/25270275/get-functions-names-in-a-shared-library-programmatically
    void* library_handle;
    rdmalib::impl::expect_nonnull(
      library_handle = dlopen(
        path.c_str(),
        RTLD_NOW
      ),
      [](){ spdlog::error(dlerror()); }
    );
    struct link_map * map = nullptr;
    dlinfo(library_handle, RTLD_DI_LINKMAP, &map);

    Elf64_Sym * symtab = nullptr;
    char * strtab = nullptr;
    int symentries = 0;
    for (auto section = map->l_ld; section->d_tag != DT_NULL; ++section)
    {
      if (section->d_tag == DT_SYMTAB)
      {
        symtab = (Elf64_Sym *)section->d_un.d_ptr;
      }
      if (section->d_tag == DT_STRTAB)
      {
        strtab = (char*)section->d_un.d_ptr;
      }
      if (section->d_tag == DT_SYMENT)
      {
        symentries = section->d_un.d_val;
      }
    }
    int size = strtab - (char *)symtab;
    for (int k = 0; k < size / symentries; ++k)
    {
      auto sym = &symtab[k];
      // If sym is function
      if (ELF64_ST_TYPE(symtab[k].st_info) == STT_FUNC)
      {
        //str is name of each symbol
        _func_names.emplace_back(&strtab[sym->st_name]);
      }
    }
    std::sort(_func_names.begin(), _func_names.end());
    dlclose(library_handle);

    return functions;
  }

  void executor::deallocate()
  {
    if(_exec_manager) {
      _end_requested = true;
      // The background thread could be nullptr if we failed in the allocation process
      if(_background_thread) {
        _background_thread->join();
        _background_thread.reset();
      }
      _exec_manager->disconnect();
      _exec_manager.reset(nullptr);
      _state._cfg.attr.send_cq = _state._cfg.attr.recv_cq = 0;

      // Clear up old connections
      _connections.clear();
    }
  }

  void executor::poll_queue()
  {
    // FIXME: hide the details in rdmalib
    spdlog::info("Background thread starts waiting for events");
    _connections[0].conn->notify_events(true);
    int flags = fcntl(_connections[0].conn->completion_channel()->fd, F_GETFL);
    int rc = fcntl(_connections[0].conn->completion_channel()->fd, F_SETFL, flags | O_NONBLOCK);
    if (rc < 0) {
      fprintf(stderr, "Failed to change file descriptor of completion event channel\n");
      return;
    }

    while(!_end_requested && _connections.size()) {
      pollfd my_pollfd;
      my_pollfd.fd      = _connections[0].conn->completion_channel()->fd;
      my_pollfd.events  = POLLIN;
      my_pollfd.revents = 0;
      do {
        rc = poll(&my_pollfd, 1, 100);
        if(_end_requested) {
          spdlog::info("Background thread stops waiting for events");
          return;
        }
      } while (rc == 0);
      if (rc < 0) {
        fprintf(stderr, "poll failed\n");
        return;
      }
      if(!_end_requested) {
        auto cq = _connections[0].conn->wait_events();
        _connections[0].conn->notify_events(true);
        _connections[0].conn->ack_events(cq, 1);
        auto wc = _connections[0].conn->receive_wcs().poll(false);
        for(int i = 0; i < std::get<1>(wc); ++i) {
          uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
          int return_val = val & 0x0000FFFF;
          int finished_invoc_id = val >> 16;
          auto it = _futures.find(finished_invoc_id);
          // if it == end -> we have a bug, should never appear
          //spdlog::info("Future for id {}", finished_invoc_id);
          //(*it).second.set_value(return_val);
          // FIXME: handle error
          if(!--std::get<0>(it->second)) {
            std::get<1>(it->second).set_value(return_val);

            _connections[0].conn->receive_wcs().update_requests(_connections.size() - 1);
            for(int i = 1; i < _connections.size(); ++i) {
              _connections[0].conn->receive_wcs().update_requests(-1);
            }
          }
        }
        // Poll completions from past sends
        for(auto & conn : _connections)
          conn.conn->poll_wc(rdmalib::QueueType::SEND, false);
      }
    }
    spdlog::info("Background thread stops waiting for events");

    // Wait for event
    // Ask for next events
    // Check if no one is polling
    // Check data
    //while(!_end_requested && _connections.size()) {
      //std::cout << ("Sleep \n");
      //auto cq = _connections[0].conn->wait_events();
      //std::cout << ("Wake up + " + std::to_string(_end_requested) + "\n");
      //if(_end_requested)
      //  break; 
      //if(_connections.size() > 0) {
      //  //std::cout << ("Check connections\n");
      //  _connections[0].conn->notify_events(true);
      //  _connections[0].conn->ack_events(cq, 1);
      //  //spdlog::info("wake up! {}", _active_polling);
      //  if(!_active_polling) {
      //    auto wc = _connections[0]._rcv_buffer.poll(false);
      //    for(int i = 0; i < std::get<1>(wc); ++i) {
      //      uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
      //      int return_val = val & 0x0000FFFF;
      //      int finished_invoc_id = val >> 16;
      //      auto it = _futures.find(finished_invoc_id);
      //      // if it == end -> we have a bug, should never appear
      //      //spdlog::info("Future for id {}", finished_invoc_id);
      //      (*it).second.set_value(return_val);
      //      // FIXME: handle error
      //    }
      //    // Poll completions from past sends
      //    _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, false);
      //  }
      //}
    //}
    //spdlog::info("Background thread stops waiting for events");
  }

  bool executor::allocate(std::string functions_path, int max_input_size,
      int hot_timeout, bool skip_manager, bool skip_resource_manger, rdmalib::Benchmarker<5> * benchmarker)
  {
    rdmalib::Buffer<char> functions = load_library(functions_path);

    if(!skip_manager) {

      // Measure connection time
      if(benchmarker)
        benchmarker->start();
      bool ret = _exec_manager->connect();
      spdlog::error("connect");
      if(benchmarker) {
        benchmarker->end(0);
        benchmarker->start();
      }
      if(!ret)
        return false;

      _exec_manager->request() = (rfaas::AllocationRequest) {
        static_cast<int32_t>(_lease_id),
        static_cast<int16_t>(hot_timeout),
        // FIXME: timeout
        5,
        // FIXME: variable number of inputs
        1,
        max_input_size,
        functions.data_size(),
        _state.listen_port(),
        ""
      };
      strcpy(_exec_manager->request().listen_address, _device.ip_address.c_str());

      // Legacy path
      if(skip_resource_manger) {

        spdlog::error("{} {}", _numcores, _memory);
        _exec_manager->request().cores = _numcores;
        _exec_manager->request().memory = _memory;

      }

      if(!_exec_manager->submit()) {
        return false;
      }
      // Measure submission time
      if(benchmarker) {
        benchmarker->end(1);
        benchmarker->start();
      }

    }

    SPDLOG_DEBUG("Allocating {} threads on a remote executor", _numcores);
    // Now receive the connections from executors
    uint32_t obj_size = sizeof(rdmalib::BufferInformation);

    // FIXME: use shared queue!

    // Accept connect requests, fill receive buffers and accept them.
    // When the connection is established, then send data.
    this->_connections.reserve(_numcores);
    int requested = 0, established = 0;
    while(established < _numcores) {

      //while(conn_status != rdmalib::ConnectionStatus::REQUESTED)
      auto [conn, conn_status] = _state.poll_events();
      if(conn_status == rdmalib::ConnectionStatus::REQUESTED) {
        SPDLOG_DEBUG(
          "[Executor] Requested connection from executor {}, connection {}",
          requested + 1, fmt::ptr(conn)
        );
        this->_connections.emplace_back(
          conn,
          _device.default_receive_buffer_size
        );
        this->_connections.back().conn->post_recv(_execs_buf.sge(obj_size, requested*obj_size), requested);
        // FIXME: here it won't work if rcv_bufer_size < numcores
        this->_connections.back().conn->receive_wcs().refill();

        _state.accept(this->_connections.back().conn.get());
        ++requested;
      } else if(conn_status == rdmalib::ConnectionStatus::ESTABLISHED) {
        SPDLOG_DEBUG(
          "[Executor] Established connection to executor {}, connection {}",
          established + 1, fmt::ptr(conn)
        );
        conn->post_send(functions);
        SPDLOG_DEBUG("Connected thread {}/{} and submitted function code.", established + 1, _numcores);
        ++established;
      }
      // FIXME: fix handling of disconnection
      else {
        spdlog::error("Unhandled connection event {} in executor allocation", conn_status);
      }
    }

    // Measure process spawn time
    if(benchmarker) {
      benchmarker->end(2);
      benchmarker->start();
    }

    // Now receive buffer information
    int received = 0;
    while(received < _numcores) {
      auto wcs = this->_connections[0].conn->poll_wc(rdmalib::QueueType::RECV, true); 
      for(int i = 0; i < std::get<1>(wcs); ++i) {
        int id = std::get<0>(wcs)[i].wr_id;
        SPDLOG_DEBUG(
          "Received buffer details for thread, addr {}, rkey {}",
          _execs_buf.data()[id].r_addr, _execs_buf.data()[id].r_key
        );
        _connections[id].remote_input = rdmalib::RemoteBuffer(
          _execs_buf.data()[id].r_addr,
          _execs_buf.data()[id].r_key
        );
      }
      received += std::get<1>(wcs);
    }

    received = 0;
    _active_polling = false;
    // Ensure that we are able to process asynchronous replies
    // before we start any submissionk.
    _connections[0].conn->notify_events(true);
    // FIXME: extend to multiple connections
    _background_thread.reset(
      new std::thread{
        &executor::poll_queue,
        this
      }
    );
    while(received < _numcores) {
      auto wcs = this->_connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);
      received += std::get<1>(wcs);
    }
    // Measure initial configuration submission
    if(benchmarker) {
      benchmarker->end(3);
      benchmarker->start();
    }
    //if(_background_thread) {
    //  _background_thread->detach();
    //}
    SPDLOG_DEBUG("Code submission for all threads is finished");
    return true;
  }

}
