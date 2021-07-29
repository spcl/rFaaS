
#include "rdmalib/rdmalib.hpp"
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

namespace rfaas {

  executor_state::executor_state(std::unique_ptr<rdmalib::Connection> conn, int rcv_buf_size):
    conn(std::move(conn)),
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
    _invoc_id(0),
    _max_inlined_msg(max_inlined_msg)
  {
    _execs_buf.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    events = 0;
  }

  executor::executor(device_data & dev):
    executor(dev.ip_address, dev.port, dev.default_receive_buffer_size, dev.max_inline_data)
  {}

  rdmalib::Buffer<char> executor::load_library(std::string path)
  {
    _func_names.clear();
    // Load the shared library with functions code
    FILE* file = fopen(path.c_str(), "rb");
    fseek (file, 0 , SEEK_END);
    size_t len = ftell(file);
    rewind(file);
    rdmalib::Buffer<char> functions(len);
    fread(functions.data(), 1, len, file);
    functions.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE);

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

    return functions;
  }

  void executor::deallocate()
  {
    if(_exec_manager) {
      _exec_manager->disconnect();
      _exec_manager.reset(nullptr);
      _connections.clear();
      _state._cfg.attr.send_cq = _state._cfg.attr.recv_cq = 0;
      _background_thread->detach();
      _background_thread.reset();
      spdlog::info("events {}", events);
    }
  }

  void executor::poll_queue()
  {
    spdlog::info("Background thread starts waiting for events");
    _connections[0].conn->notify_events(true);
    // Wait for event
    // Ask for next events
    // Check if no one is polling
    // Check data
    while(_connections.size()) {
      auto cq = _connections[0].conn->wait_events();
      _connections[0].conn->notify_events(true);
      _connections[0].conn->ack_events(cq, 1);
      if(!_active_polling) {
        auto wc = _connections[0]._rcv_buffer.poll(false);
        for(int i = 0; i < std::get<1>(wc); ++i) {
          uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
          int return_val = val & 0x0000FFFF;
          int finished_invoc_id = val >> 16;
          auto it = _futures.find(finished_invoc_id);
          // if it == end -> we have a bug, should never appear
          //spdlog::info("Future for id {}", finished_invoc_id);
          (*it).second.set_value(return_val);
          // FIXME: handle error
        }
      }
    }
    spdlog::info("Background thread stops waiting for events");
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

    for(int i = 0; i < numcores; ++i) {
      this->_connections.emplace_back(
        _state.poll_events(
          true
        ),
        _rcv_buf_size
      );
      this->_connections.back().conn->post_recv(_execs_buf.sge(obj_size, i*obj_size), i);
      _state.accept(this->_connections.back().conn);
      this->_connections.back().conn->post_send(functions);
      SPDLOG_DEBUG("Connected thread {}/{} and submitted function code.", i + 1, numcores);
      // FIXME: this should be in a function
      // FIXME: here it won't work if rcv_bufer_size < numcores
      this->_connections.back()._rcv_buffer.connect(this->_connections.back().conn.get());
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
    while(received < numcores) {
      auto wcs = this->_connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);
      received += std::get<1>(wcs);
    }
    // Measure initial configuration submission
    if(benchmarker) {
      benchmarker->end(3);
      benchmarker->start();
    }
    if(_background_thread) {
      _background_thread->detach();
    }
    // FIXME: extend to multiple connections
    _background_thread.reset(
      new std::thread{
        &executor::poll_queue,
        this
      }
    );
    SPDLOG_DEBUG("Code submission for all threads is finished");
    return true;
  }

}
