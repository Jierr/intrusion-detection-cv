#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <list>
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
	constexpr float SATURATION_TRIGGER = 0.02f;
	
	constexpr char ARG_URL[] = "--url";
	constexpr char ARG_EMAIL[] = "--email";
	constexpr char ARG_STORAGE[] = "--storage";
}

float getImageBusiness(const cv::Mat& mask);

int main(int argc, char** argv)
{
	std::string url;
	std::string email;
	std::string storage;
	
	// parse command line arguments
	ArgumentParser parser;
	parser.registerArgument(ARG_URL, "rtsp://user:password@localhost:554/stream");
	parser.registerArgument(ARG_EMAIL, "my@address.com");
	parser.registerArgument(ARG_STORAGE, ".");
	parser.parse(argc, argv);
	
	auto argUrl = parser[ARG_URL];
	auto argEmail = parser[ARG_EMAIL];
	auto argStorage = parser[ARG_STORAGE];
	
	if (argUrl.exists) url = argUrl.value;
	if (argEmail.exists) email = argEmail.value;
	if (argStorage.exists) storage = argStorage.value;
	
	std::cerr << ARG_URL << " = " << url << std::endl;
	std::cerr << ARG_EMAIL << " = " << email << std::endl;
	std::cerr << ARG_STORAGE << " = " << storage << std::endl;
	
	cv::VideoCapture capture;
	if(url.empty())
	{
		// Use default (Webcam)
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
	cv::namedWindow("ForegroundSmoothed", cv::WINDOW_AUTOSIZE);
	
    cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorMOG2(FPS * 60 * 5, 32, true);
    //cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorKNN(FPS * 60 * 5, 100, true);
    if (!backgroundSubtractor)
    {
		std::cerr << "Could not create backgound subtractor..." << std::endl;
		return -1;    
    }
	std::chrono::system_clock::time_point trigger = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point currentTime;
	
	// Set from, to
	EMailNotifier notifier(email, email);
	
	// Set location for videos & images to be stored.
	Persistence persistence(storage);
	bool running{true};	
	char isoTime[255] = {0};		
	cv::Mat frame, frameSmoothed, foregroundMask, foregroundMaskSmoothed;
	while(running) 
	{		
		
		if (!capture.read(frame))
		{
			std::cerr << "Could not read frame..." << std::endl;
			continue;
		}
		
		// Create Background from noise reduced image
		cv::blur(frame, frameSmoothed, cv::Size(3,3));
        backgroundSubtractor->apply(frameSmoothed, foregroundMask);
		cv::imshow("Foreground", foregroundMask);
		
		// Reduce Noice on forground mask
        cv::erode(foregroundMask, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
        cv::dilate(foregroundMaskSmoothed, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
		cv::imshow("Surveillance", frameSmoothed);
		cv::imshow("ForegroundSmoothed", foregroundMaskSmoothed);
		
		// Evaluate image foreground saturation 
		float saturation = getImageBusiness(foregroundMaskSmoothed);
		
		// check, if movement was detected		
		currentTime = std::chrono::system_clock::now();
		if (saturation > SATURATION_TRIGGER && (currentTime > trigger + TRIGGER_DELAY_SECONDS))
		{
			// Reset timer for next trigger condition
			trigger = std::chrono::system_clock::now();
			std::time_t cTrigger = std::chrono::system_clock::to_time_t(trigger);
			std::cout << "Possible Intrusion: " << std::ctime(&cTrigger) << std::endl;		
			
			// compose alert message
			{			
				std::stringstream compose;
				
				// Subject, currently colons are erronous during image creation.
  				strftime(isoTime, sizeof(isoTime),"%F", std::localtime(&cTrigger));	
				compose << "Intrusion detected " << isoTime;
				std::string subject = compose.str();
				
				// Body
  				strftime(isoTime, sizeof(isoTime),"%FT%X", std::localtime(&cTrigger));
				compose.str("");
				compose << "Possible Intrusion detected (" << isoTime << ") with an image saturation of " 
						<< saturation * 100.0 << "%. "
						<< "Please see attachment for detected image area.";
				std::string body = compose.str();
				std::cout << body << std::endl;				
				
				// Save triggering images fullfilling saturation condition.
				std::string jpgReal = std::string(isoTime) + std::string("-real.jpg");
				std::string jpgForeground = std::string(isoTime) + std::string("-foreground.jpg");
				persistence.saveJpg(frame, jpgReal);
				persistence.saveJpg(foregroundMask, jpgForeground);
				std::list<std::string> attachments{jpgReal, jpgForeground};
				
				// Send EMail
				notifier.alert(subject, body, storage, attachments);	
			}
		}
		
		// Wait and Evaluate pressed keys.
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
	
	// Cleanup
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

