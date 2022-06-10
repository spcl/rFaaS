
#include "simulator.hpp"

namespace simulator {

  void RequestMessage::set_allocation(int allocation)
  {
    _buffer = allocation;
  }

  int RequestMessage::get_allocation()
  {
    return _buffer;
  }

  MPI_Datatype RequestMessage::datatype()
  {
    return MPI_INT;
  }

  void RequestMessage::send_allocation(int executor)
  {
    MPI_Send(&_buffer, 1, datatype(), executor, _tag++, _comm);
  }

  void RequestMessage::recv_allocation(MPI_Request* req)
  {
    MPI_Irecv(&_buffer, 1, datatype(), MPI_ANY_SOURCE, MPI_ANY_TAG, _comm, req);
  }

  void ReplyMessage::set_cores(int cores)
  {
    _buffer = cores;
  }

  void ReplyMessage::set_failure()
  {
    _buffer = -1;
  }

  bool ReplyMessage::succeeded()
  {
    return _buffer >= 0;
  }

  int ReplyMessage::get_cores()
  {
    return _buffer;
  }

  MPI_Datatype ReplyMessage::datatype()
  {
    return MPI_INT;
  }

  void ReplyMessage::send_reply(int client)
  {
    MPI_Send(&_buffer, 1, datatype(), client, _tag++, _comm);
  }

  void ReplyMessage::recv_reply(int executor)
  {
    MPI_Status status;
    MPI_Recv(&_buffer, 1, datatype(), executor, MPI_ANY_TAG, _comm, &status);
  }
}
