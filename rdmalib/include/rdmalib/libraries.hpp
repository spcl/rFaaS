#ifndef __RDMALIB_LIBRARIES_HPP__
#define __RDMALIB_LIBRARIES_HPP__

#include <rdma/rdma_cma.h>
#include <rdma/fabric.h>
//#include "rdmalib/rdmalib.hpp"

// Forward declare ibverbs structs
struct ibv_pd;
struct ibv_mr;
struct ibv_sge;
struct ibv_qp;
struct ibv_comp_channel;
struct ibv_wc;

// Library parameter definitions
struct ibverbs;
struct libfabric;

template <typename Library>
struct library_traits;

template <>
struct library_traits<libfabric>
{
  using type = libfabric;

  using mr_t = fid_mr *;
  using pd_t = fid_domain *;
  using lkey_t = void *;
  using rkey_t = uint64_t;
  using sge_t = iovec;

  using qp_t = fid_ep *;
  using wc_t = fi_cq_data_entry;
  using id_t = fid *;
  using channel_t = fid_cq *;

  // template <typename T>
  // using LibBuffer = rdmalib::Buffer<T, libfabric>;
  // using LibSGE = rdmalib::LibfabricScatterGatherElement;
  // using LibConnection = rdmalib::LibfabricConnection;
};

template <>
struct library_traits<ibverbs>
{
  using type = ibverbs;

  using mr_t = ibv_mr *;
  using pd_t = ibv_pd *;
  using lkey_t = uint32_t;
  using rkey_t = uint32_t;
  using sge_t = ibv_sge;

  using qp_t = ibv_qp *;
  using wc_t = ibv_wc;
  using id_t = rdma_cm_id *;
  using channel_t = ibv_comp_channel *;

  // These are going to need to go elsewhere
  // template <typename T>
  // using LibBuffer = rdmalib::Buffer<T, ibverbs>;
  // using LibSGE = rdmalib::VerbsScatterGatherElement;
  // using LibConnection = rdmalib::VerbsConnection;
};

#endif