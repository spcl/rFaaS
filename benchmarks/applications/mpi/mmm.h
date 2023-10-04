
void mmm(int n, double* a, double* b, double* c)
{
  for(int i=0; i<n; i++)
    for(int j=0; j<n; j++)
      for(int k=0; k<n; k++)
        c[i*n+j] += a[i*n+k]*b[k*n+j];
}

void mmm2(int n, double* a, double* b, double* c)
{
  int half_n = n/2;
  //int half_n = static_cast<int>(0.6 * n);
  for(int i=0; i<half_n; i++)
    for(int j=0; j<n; j++)
      for(int k=0; k<n; k++)
        c[i*n+j] += a[i*n+k]*b[k*n+j];
}

void mmm3(int n, double* a, double* b, double* c)
{
  //int half_n = static_cast<int>(0.6 * n);
  for(int i=n/2; i<n; i++)
    for(int j=0; j<n; j++)
      for(int k=0; k<n; k++)
        c[i*n+j] += a[i*n+k]*b[k*n+j];
}
