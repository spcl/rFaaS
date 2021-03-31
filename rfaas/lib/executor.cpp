
#include <spdlog/spdlog.h>

#include <rdmalib/allocation.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/util.hpp>
#include <rfaas/executor.hpp>

    // FIXME: same function as in server/functions.cpp - merge?
#include <sys/mman.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>

namespace rfaas {

  executor_state::executor_state(rdmalib::Connection* conn, int rcv_buf_size):
    conn(conn),
    _rcv_buffer(rcv_buf_size)
  {

  }

  executor::executor(std::string address, int port, int rcv_buf_size, int max_inlined_msg):
    _state(address, port, rcv_buf_size + 1),
    _rcv_buffer(rcv_buf_size),
    _rcv_buf_size(rcv_buf_size),
    _executions(0),
    _invoc_id(0),
    _max_inlined_msg(max_inlined_msg)
  {
  }

  rdmalib::Buffer<char> executor::load_library(std::string path)
  {
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

  void executor::allocate(std::string functions_path, int numcores)
  {
    // FIXME: here send cold allocations

    rdmalib::Buffer<char> functions = load_library(functions_path);

    SPDLOG_DEBUG("Allocating {} threads on a remote executor", numcores);
    // FIXME: temporary fix of vector reallocation - return Connection object?
    _state._connections.reserve(numcores);
    // Now receive the connections from executors
    rdmalib::Buffer<rdmalib::BufferInformation> buf(numcores);
    uint32_t obj_size = sizeof(rdmalib::BufferInformation);
    buf.register_memory(_state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    // FIXME: shared receive queue here - batched receive
    for(int i = 0; i < numcores; ++i) {
      // FIXME: single QP
      this->_connections.emplace_back(
        _state.poll_events(
          [this,&buf,obj_size,i](rdmalib::Connection& conn){
            conn.post_recv(buf.sge(obj_size, i*obj_size), i);
          },
          true
        ),
        _rcv_buf_size
      );
      this->_connections.back().conn->post_send(functions);
      SPDLOG_DEBUG("Connected thread {}/{} and submitted function code.", i + 1, numcores);
      // FIXME: this should be in a function
      this->_connections.back()._rcv_buffer.connect(this->_connections.back().conn);
    }

    // Now receive buffer information
    int received = 0;
    while(received < numcores) {
      // FIXME: single QP
      auto wcs = this->_connections[0].conn->poll_wc(rdmalib::QueueType::RECV, true); 
      for(int i = 0; i < std::get<1>(wcs); ++i) {
        int id = std::get<0>(wcs)[i].wr_id;
        SPDLOG_DEBUG(
          "Received buffer details for thread, addr {}, rkey {}",
          buf.data()[id].r_addr, buf.data()[id].r_key
        );
        _connections[id].remote_input = rdmalib::RemoteBuffer(buf.data()[id].r_addr, buf.data()[id].r_key);
      }
      received += std::get<1>(wcs);
    }

    // FIXME: Ensure that function code has been submitted
    received = 0;
    while(received < numcores) {
      auto wcs = this->_connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);
      received += std::get<1>(wcs);
    }
    SPDLOG_DEBUG("Code submission for all threads is finished");
  }

}
