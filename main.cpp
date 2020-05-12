#include <iostream>
#include <string>

#include <opencv2/video/video.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

using namespace std;
using namespace cv;

int main(int argc, char** argv)
{
	std::string url;
	if (argc >= 3)
	{
		if(std::string("--url") == argv[1])
		{
			url = std::string(argv[2]);
		}
	}

	cv::VideoCapture capture(url);

	if (!capture.isOpened()) 
	{
		std::cerr << "Cannot open " << url;
		return -1;
	}
	std::cerr << "Opened " << url;
	
	cv::namedWindow("LOW", cv::WINDOW_AUTOSIZE);
	cv::Mat frame;

	while(true) 
	{
		if (!capture.read(frame))
		{
			std::cerr << "Cannot read frame";
		}
		cv::imshow("LOW", frame);
		cv::waitKey(30);
	}
	return 0;
}
