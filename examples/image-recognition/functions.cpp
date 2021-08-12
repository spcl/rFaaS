
#include <vector>
#include <cstdint>

#include "function.hpp"

extern "C" uint32_t image_recognition(void* args, uint32_t size, void* res)
{
  char* input = static_cast<char*>(args);
  int* output = static_cast<int*>(res);
  std::vector<unsigned char> vectordata(input, input + size);
  cv::Mat image = imdecode(cv::Mat(vectordata), 1);
  cv::Mat image2;
  *output = recognition(image);
  //fprintf(stderr, "%d %d\n", image2.rows, image2.cols);
  //std::vector<unsigned char> out_buffer;
  //cv::imencode(".jpg", image2, out_buffer);
  //memcpy(output,out_buffer.data(), out_buffer.size());

  return sizeof(int);
}

