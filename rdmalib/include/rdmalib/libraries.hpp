#ifndef __RDMALIB_LIBRARIES_HPP__
#define __RDMALIB_LIBRARIES_HPP__

#include <rdma/rdma_cma.h>
#include <rdma/fabric.h>
#include <rdma/fi_eq.h>

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

// Base trait
template <typename Library>
struct library_traits;

// Library-specialized traits
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
};

namespace rdmalib {
  template <typename Derived, typename Library>
  struct ScatterGatherElement;

  struct LibfabricScatterGatherElement;
  struct VerbsScatterGatherElement;

  /*
  template <typename Lib>
  struct sge_trait;

  template <>
  struct sge_trait<libfabric>
  {
    using ScatterGatherElement = LibfabricScatterGatherElement;
  };

  template <>
  struct sge_trait<ibverbs>
  {
    using ScatterGatherElement = VerbsScatterGatherElement;
  };
  */

  template <typename Library>
  struct rdmalib_traits;

  struct LibfabricConnection;
  struct VerbsConnection;

  struct LibfabricRemoteBuffer;
  struct VerbsRemoteBuffer;

  struct LibfabricAddress;
  struct VerbsAddress;

  struct LibfabricRDMAActive;
  struct VerbsRDMAActive;
  struct LibfabricRDMAPassive;
  struct VerbsRDMAPassive;

  struct LibfabricRecvBuffer;
  struct VerbsRecvBuffer;

  template <>
  struct rdmalib_traits<libfabric> {
    using Connection = LibfabricConnection;
    using Address = LibfabricAddress;
    using RDMAActive = LibfabricRDMAActive;
    using RDMAPassive = LibfabricRDMAPassive;
    using RecvBuffer = LibfabricRecvBuffer;
    using ScatterGatherElement = LibfabricScatterGatherElement;
    using RemoteBuffer = LibfabricRemoteBuffer;
  };

  template <>
  struct rdmalib_traits<ibverbs> {
    using Connection = VerbsConnection;
    using Address = VerbsAddress;
    using RDMAActive = VerbsRDMAActive;
    using RDMAPassive = VerbsRDMAPassive;
    using RecvBuffer = VerbsRecvBuffer;
    using ScatterGatherElement = VerbsScatterGatherElement;
    using RemoteBuffer = VerbsRemoteBuffer;
  };
}

#endif
