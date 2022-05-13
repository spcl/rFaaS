module unload PrgEnv-cray
module load PrgEnv-gnu
module load rdma-credentials
srun -l -N1 -n1 benchmarks/credentials