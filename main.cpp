#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <sstream>

#include <opencv2/video/video.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

#include "ArgumentParser.hpp"
#include "EMailNotifier.hpp"
#include "Persistence.hpp"

using namespace std;
using namespace cv;

namespace
{
	constexpr int FPS(24);
	constexpr int MS_PER_FRAME (1000/FPS);
	constexpr int VK_ESCAPE{27};
	constexpr auto TRIGGER_DELAY_SECONDS = std::chrono::seconds(10);
	constexpr float SATURATION_TRIGGER = 0.035f;
	
	constexpr char ARG_URL[] = "--url";
	constexpr char ARG_EMAIL[] = "--email";
	constexpr char ARG_EPASSWORD[] = "--epassword";
	constexpr char ARG_STORAGE[] = "--storage";
}

float getImageBusiness(const cv::Mat& mask);

int main(int argc, char** argv)
{
	std::string url;
	std::string email;
	std::string password;
	std::string storage;
	
	ArgumentParser parser;
	parser.registerArgument(ARG_URL, "rtsp://user:password@localhost:554/stream");
	parser.registerArgument(ARG_EMAIL, "my@address.com");
	parser.registerArgument(ARG_EPASSWORD, "");
	parser.registerArgument(ARG_STORAGE, ".");
	parser.parse(argc, argv);
	
	auto argUrl = parser[ARG_URL];
	auto argEmail = parser[ARG_EMAIL];
	auto argEPassword = parser[ARG_EPASSWORD];
	auto argStorage = parser[ARG_STORAGE];
	
	if (argUrl.exists) url = argUrl.value;
	if (argEmail.exists) email = argEmail.value;
	if (argEPassword.exists) password = argEPassword.value;
	if (argStorage.exists) storage = argStorage.value;
	
	std::cerr << ARG_URL << " = " << url << std::endl;
	std::cerr << ARG_EMAIL << " = " << email << std::endl;
	
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
	cv::namedWindow("Foreground1", cv::WINDOW_AUTOSIZE);
	cv::namedWindow("ForegroundSmoothed", cv::WINDOW_AUTOSIZE);
	
    cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorMOG2(FPS * 60 * 5, 32, true);
    //cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorKNN(FPS * 60 * 5, 100, true);
    if (!backgroundSubtractor)
    {
		std::cerr << "Could not create backgound subtractor..." << std::endl;
		return -1;    
    }
	cv::Mat frame, frameSmoothed, foregroundMask, foregroundMaskSmoothed;
	bool running{true};
	std::chrono::system_clock::time_point trigger = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point currentTime;
	
	EMailNotifier notifier(email, email, password);
	Persistence persistence(storage);
	while(running) 
	{
		if (!capture.read(frame))
		{
			std::cerr << "Could not read frame..." << std::endl;
			continue;
		}
		
		cv::blur(frame, frameSmoothed, cv::Size(3,3));
        backgroundSubtractor->apply(frameSmoothed, foregroundMask);
        
		cv::imshow("Foreground1", foregroundMask);
        cv::erode(foregroundMask, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
        cv::dilate(foregroundMaskSmoothed, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
		cv::imshow("Surveillance", frame);
		cv::imshow("ForegroundSmoothed", foregroundMaskSmoothed);
		float saturation = getImageBusiness(foregroundMaskSmoothed);
		// check, if movement was detected		
		currentTime = std::chrono::system_clock::now();
		if (saturation > SATURATION_TRIGGER && (currentTime > trigger + TRIGGER_DELAY_SECONDS))
		{
			trigger = std::chrono::system_clock::now();
			std::time_t cTrigger = std::chrono::system_clock::to_time_t(trigger);
			std::cout << "Possible Intrusion: " << std::ctime(&cTrigger) << std::endl;	
			
			// compose alert message
			{			
				std::stringstream compose;
				compose << "Intrusion detected " << std::ctime(&cTrigger);
				std::string subject = compose.str();
				compose.str("");
				compose << "Possible Intrusion detected with an image saturation of " 
						<< saturation * 100.0 << "%. "
						<< "Please see attachment for detected image area.";
				std::string body = compose.str();
				persistence.saveJpg(frame, std::string("real-") + std::to_string(trigger.time_since_epoch().count()) + ".jpg");
				persistence.saveJpg(foregroundMask, std::string("foreground-") + std::to_string(trigger.time_since_epoch().count()) + ".jpg");
				notifier.alert(subject, body, "");	
			}
		}
		
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
		}
	}
	return static_cast<float>(saturation) / static_cast<float>(maxSaturation); 
}

