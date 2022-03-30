
#include "rdmalib/rdmalib.hpp"
#include <chrono>
#include <spdlog/spdlog.h>

#include <rdmalib/allocation.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/util.hpp>

#include <rfaas/connection.hpp>
#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>

// FIXME: same function as in server/functions.cpp - merge?
#include <sys/mman.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <poll.h>
#include <thread>

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
    conn(conn),
    _rcv_buffer(rcv_buf_size)
  {
  }

  executor::executor(std::string address, int port, int rcv_buf_size, int max_inlined_msg):
    _state(address, port, rcv_buf_size + 1),
    _rcv_buffer(rcv_buf_size),
    _execs_buf(MAX_REMOTE_WORKERS),
    _address(address),
    _port(port),
    _rcv_buf_size(rcv_buf_size),
    _executions(0),
    #ifdef USE_LIBFABRIC
    _invoc_id(1),
    #else
    _invoc_id(0),
    #endif
    _max_inlined_msg(max_inlined_msg),
    _perf(1000)
  {
    #ifdef USE_LIBFABRIC
    _execs_buf.register_memory(_state.pd(), FI_WRITE | FI_REMOTE_WRITE);
    #else
    _execs_buf.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    #endif
    events = 0;
    _active_polling = false;
    _end_requested = false;
  }

  executor::executor(device_data & dev):
    executor(dev.ip_address, dev.port, dev.default_receive_buffer_size, dev.max_inline_data)
  {}

  executor::~executor()
  {
    this->deallocate();
    _perf.export_csv("client_perf.csv", {"start", "function parsed", "function post written", "buffer refilled", "received result", "parsed result", "catched unlikely case", "polled send"});
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
    #ifdef USE_LIBFABRIC
    functions.register_memory(_state.pd(), FI_WRITE);
    #else
    functions.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);
    #endif
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
      #ifndef USE_LIBFABRIC
      _state._cfg.attr.send_cq = _state._cfg.attr.recv_cq = 0;
      #endif

      // Clear up old connections
      _connections.clear();
    }
  }

  void executor::poll_queue()
  {
    // FIXME: hide the details in rdmalib
    spdlog::info("Background thread starts waiting for events");
    #ifdef USE_LIBFABRIC
    int rc;
    #else
    _connections[0].conn->notify_events(true);
    int flags = fcntl(_connections[0].conn->completion_channel()->fd, F_GETFL);
    int rc = fcntl(_connections[0].conn->completion_channel()->fd, F_SETFL, flags | O_NONBLOCK);
    if (rc < 0) {
      fprintf(stderr, "Failed to change file descriptor of completion event channel\n");
      return;
    }
    #endif

    while(!_end_requested && _connections.size()) {
      #ifdef USE_LIBFABRIC
      do {
        if(_active_polling)
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        else
          rc = _connections[0].conn->wait_events(100);
        if(_end_requested) {
          spdlog::info("Background thread stops waiting for events");
          return;
        }
      } while (rc != 0);
      #else
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
      #endif
      if (rc < 0) {
        fprintf(stderr, "poll failed\n");
        return;
      }
      if(!_end_requested && !_active_polling) {
        #ifndef USE_LIBFABRIC
        auto cq = _connections[0].conn->wait_events();
        _connections[0].conn->notify_events(true);
        _connections[0].conn->ack_events(cq, 1);
        #endif
        #ifdef USE_LIBFABRIC
        auto wc = _connections[0].conn->poll_wc(rdmalib::QueueType::RECV, false);
        #else
        auto wc = _connections[0]._rcv_buffer.poll(false);
        #endif
        for(int i = 0; i < std::get<1>(wc); ++i) {
          #ifdef USE_LIBFABRIC
          uint32_t val = std::get<0>(wc)[i].data;
          #else
          uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
          #endif
          int return_val = val & 0x0000FFFF;
          int finished_invoc_id = val >> 16;
          auto it = _futures.find(finished_invoc_id);
          // if it == end -> we have a bug, should never appear
          //spdlog::info("Future for id {}", finished_invoc_id);
          //(*it).second.set_value(return_val);
          // FIXME: handle error
          if(!--std::get<0>(it->second)) {
            std::get<1>(it->second).set_value(return_val);
            // FIXME
            //
            _connections[0]._rcv_buffer._requests += _connections.size() - 1;
            for(size_t i = 1; i < _connections.size(); ++i)
              _connections[i]._rcv_buffer._requests--;
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

  bool executor::allocate(std::string functions_path, int numcores, int max_input_size,
      int hot_timeout, bool skip_manager, rdmalib::Benchmarker<5> * benchmarker)
  {
    rdmalib::Buffer<char> functions = load_library(functions_path);
    if(!skip_manager) {
      // FIXME: handle more than one manager
      servers & instance = servers::instance();
      auto selected_servers = instance.select(numcores);

      _exec_manager.reset(
        new manager_connection(
          instance.server(selected_servers[0]).address,
          instance.server(selected_servers[0]).port,
          _rcv_buf_size,
          _max_inlined_msg
        )
      );
      // Measure connection time
      if(benchmarker)
        benchmarker->start();
      bool ret = _exec_manager->connect();
      if(benchmarker) {
        benchmarker->end(0);
        benchmarker->start();
      }
      if(!ret)
        return false;

      _exec_manager->request() = (rdmalib::AllocationRequest) {
        static_cast<int16_t>(hot_timeout),
        // FIXME: timeout
        5,
        static_cast<int16_t>(numcores),
        // FIXME: variable number of inputs
        1,
        max_input_size,
        functions.data_size(),
        _port,
        ""
      };
      strcpy(_exec_manager->request().listen_address, _address.c_str());
      _exec_manager->submit();
      // Measure submission time
      if(benchmarker) {
        benchmarker->end(1);
        benchmarker->start();
      }
    }

    SPDLOG_DEBUG("Allocating {} threads on a remote executor", numcores);
    // Now receive the connections from executors
    uint32_t obj_size = sizeof(rdmalib::BufferInformation);

    // Accept connect requests, fill receive buffers and accept them.
    // When the connection is established, then send data.
    this->_connections.reserve(numcores);
    int requested = 0, established = 0;
    while(established < numcores) {

      //while(conn_status != rdmalib::ConnectionStatus::REQUESTED)
      auto [conn, conn_status] = _state.poll_events(true);
      if(conn_status == rdmalib::ConnectionStatus::REQUESTED) {
        SPDLOG_DEBUG(
          "[Executor] Requested connection from executor {}, connection {}",
          requested + 1, fmt::ptr(conn)
        );
        this->_connections.emplace_back(
          conn,
          _rcv_buf_size
        );
        #ifdef USE_LIBFABRIC
        this->_connections.back().conn->post_recv(_execs_buf.sge(obj_size, requested*obj_size), requested);
        #else
        this->_connections.back().conn->post_recv(_execs_buf.sge(obj_size, requested*obj_size), requested);
        // FIXME: this should be in a function
        // FIXME: here it won't work if rcv_bufer_size < numcores
        this->_connections.back()._rcv_buffer.connect(this->_connections.back().conn.get());
        #endif
        _state.accept(this->_connections.back().conn.get());
        ++requested;
      } else if(conn_status == rdmalib::ConnectionStatus::ESTABLISHED) {
        SPDLOG_DEBUG(
          "[Executor] Established connection to executor {}, connection {}",
          established + 1, fmt::ptr(conn)
        );
        conn->post_send(functions);
        SPDLOG_DEBUG("Connected thread {}/{} and submitted function code.", established + 1, numcores);
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
    while(received < numcores) {
      auto wcs = this->_connections[0].conn->poll_wc(rdmalib::QueueType::RECV, true); 
      for(int i = 0; i < std::get<1>(wcs); ++i) {
        #ifdef USE_LIBFABRIC
        int id = reinterpret_cast<uint64_t>(std::get<0>(wcs)[i].op_context);
        #else
        int id = std::get<0>(wcs)[i].wr_id;
        #endif
        SPDLOG_DEBUG(
          "Received buffer details for thread, id {}, addr {}, rkey {}",
          id, _execs_buf.data()[id].r_addr, _execs_buf.data()[id].r_key
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
    #ifndef USE_LIBFABRIC
    _connections[0].conn->notify_events(true);
    #endif
    // FIXME: extend to multiple connections
    _background_thread.reset(
      new std::thread{
        &executor::poll_queue,
        this
      }
    );
    while(received < numcores) {
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
