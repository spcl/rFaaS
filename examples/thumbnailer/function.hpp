
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

void thumbnailer(cv::Mat & in, cv::Mat & out)
{
	int down_width = 200;
	int down_height = 200;
  cv::resize(in, out, cv::Size(down_width, down_height), cv::INTER_LINEAR);
}

