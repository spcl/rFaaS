
#include <chrono>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <rfaas/allocation.hpp>

#include "client.hpp"

namespace rfaas::resource_manager {

  Client::Client(int client_id, rdmalib::Connection* conn, ibv_pd* pd):
    connection(conn),
    _response(1),
    allocation_requests(RECV_BUF_SIZE),
    allocation_time(0),
    client_id(client_id)
  {
    // Make the buffer accessible to clients
    allocation_requests.register_memory(pd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    _response.register_memory(pd, IBV_ACCESS_LOCAL_WRITE);

    // Initialize batch receive WCs
    connection->receive_wcs().initialize(allocation_requests);
  }

  Client::~Client()
  {
    if(connection) {
      connection->close();
      delete connection;
    }
  }

  Client::Client(Client&& obj)
  {
    _move(std::move(obj));
  }

  Client& Client::operator=(Client&& obj)
  {
    _move(std::move(obj));
    return *this;
  }

  void Client::_move(Client&& obj)
  {
    this->connection = obj.connection;
    obj.connection = nullptr;

    this->_response = std::move(obj._response);
    this->allocation_requests = std::move(obj.allocation_requests);

    this->allocation_time = obj.allocation_time;
    obj.allocation_time = 0;
    this->client_id = obj.client_id;
    obj.client_id = 0;

    this->_cur_allocation_start = obj._cur_allocation_start;
  }

  rdmalib::Buffer<rfaas::LeaseResponse>& Client::response()
  {
    return _response;
  }

  void Client::disable()
  {
    rdma_disconnect(connection->id());
    connection->close();
    delete connection;
    connection = nullptr;
  }

  void Client::reload_queue()
  {
    connection->receive_wcs().refill();
  }

  void Client::begin_allocation()
  {
    _cur_allocation_start = std::chrono::high_resolution_clock::now();
  }

  void Client::end_allocation()
  {
    auto cur_allocation_end = std::chrono::high_resolution_clock::now();
    allocation_time += std::chrono::duration_cast<std::chrono::microseconds>(cur_allocation_end - _cur_allocation_start).count();
  }

}

