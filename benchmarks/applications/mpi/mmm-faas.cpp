#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>

extern "C" uint32_t mmm(void* args, uint32_t size, void* res)
{
  int n = sqrt(size/(sizeof(double)*2));
  printf("Received %d bytes, sending %d, processing %d", size, size/2, n);
  double* A = reinterpret_cast<double*>(args);
  double* B = A + n*n;
  double* C = reinterpret_cast<double*>(res);
  for(int i=0; i<n; i++)
    for(int j=0; j<n; j++)
      for(int k=0; k<n; k++)
        C[i*n+j] += A[i*n+k]*B[k*n+j];
  return size/2;
}
extern "C" uint32_t mmm3(void* args, uint32_t size, void* res)
{
  int n = sqrt(size/(sizeof(double)*2));
  printf("Received %d bytes, sending %d, processing %d", size, size/2, n);
  double* A = reinterpret_cast<double*>(args);
  double* B = A + n*n;
  double* C = reinterpret_cast<double*>(res);
  memset(C, 0, sizeof(double) * n * n);
  int half_n = n/2;
  //int half_n = n - static_cast<int>(0.6 * n);
  for(int i=half_n; i<n; i++)
    for(int j=0; j<n; j++)
      for(int k=0; k<n; k++)
        C[i*n+j] += A[i*n+k]*B[k*n+j];
  return size/2;
}
