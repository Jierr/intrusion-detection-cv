#include <iostream>
#include <string>
#include <memory>

#include <opencv2/video/video.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

using namespace std;
using namespace cv;

constexpr int FPS(24);
constexpr int MS_PER_FRAME (1000/FPS);
constexpr int VK_ESCAPE{27};


float getImageBusiness(const cv::Mat& mask);

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
	
	cv::VideoCapture capture;
	if(url.empty())
	{
		capture.open(0);
	}
	else
	{
		capture.open(url);
	}
	
	if (!capture.isOpened()) 
	{
		std::cerr << "Cannot open \"" << url << "\"" << std::endl;
		return -1;
	}
	std::cerr << "Opened \"" << url << "\"" << std::endl;
	
	cv::namedWindow("Surveillance", cv::WINDOW_AUTOSIZE);
	cv::namedWindow("Foreground", cv::WINDOW_AUTOSIZE);
	cv::namedWindow("Foreground2", cv::WINDOW_AUTOSIZE);
	
    cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorMOG2(FPS * 60 * 1, 32, true);
    //cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorKNN(FPS * 60 * 5, 100, true);
    if (!backgroundSubtractor)
    {
		std::cerr << "Could not create backgound subtractor..." << std::endl;
		return -1;    
    }
	cv::Mat frame, foregroundMask;
	bool running{true};
	while(running) 
	{
		if (!capture.read(frame))
		{
			std::cerr << "Could not read frame..." << std::endl;
			continue;
		}
		
        backgroundSubtractor->apply(frame, foregroundMask);
        
		cv::imshow("Foreground1", foregroundMask);
        cv::erode(foregroundMask, foregroundMask, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
        cv::dilate(foregroundMask, foregroundMask, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
		cv::imshow("Surveillance", frame);
		cv::imshow("Foreground2", foregroundMask);
		float saturation = getImageBusiness(foregroundMask);
		std::cout << "Image saturation = " << saturation * 100.0 << "%" << std::endl;
		int key = cv::waitKey(MS_PER_FRAME);
		switch(key)
		{
			case VK_ESCAPE:
				std::cout << "Terminating..." << std::endl;
				running = false;
				break;
			default:
				break;				
		}
	}
	
	capture.release();
	cv::destroyAllWindows();
	return 0;
}

float getImageBusiness(const cv::Mat& mask)
{
	const int maxSaturation = mask.rows * mask.cols;
	int saturation = 0;
	for(int i=0; i<mask.rows; i++)
	{
		for(int j=0; j<mask.cols; j++) 
		{
			if (mask.at<unsigned char>(i,j) != 0)
			{
				++saturation;
			}
		    //std::cout << mask.at<unsigned char>(i,j) << std::endl;
		}
	}
	return static_cast<float>(saturation) / static_cast<float>(maxSaturation); 
}

