#ifndef __RDMALIB_LIBRARIES_HPP__
#define __RDMALIB_LIBRARIES_HPP__

#include <rdma/fabric.h>

struct ibv_pd;
struct ibv_mr;
struct ibv_sge;

struct ibverbs;
struct libfabric;

template <typename Library>
struct library_traits;

template <>
struct library_traits<ibverbs>
{
  typedef ibverbs type;

  typedef ibv_mr *mr_t;
  typedef ibv_pd *pd_t;
  typedef uint32_t lkey_t;
  typedef uint32_t rkey_t;
  typedef ibv_sge sge_t;

  typedef ibv_qp *qp_t;
  typedef ibv_comp_channel *channel_t;
  typedef ibv_wc wc_t;
};

template <>
struct library_traits<libfabric>
{
  typedef libfabric type;

  typedef fid_mr *mr_t;
  typedef fid_domain *pd_t;
  typedef void *lkey_t;
  typedef uint64_t rkey_t;
  typedef iovec sge_t;

  typedef fid_ep *qp_t;
  typedef fid_cq *channel_t;
  typedef fi_cq_data_entry wc_t;
};

#endif