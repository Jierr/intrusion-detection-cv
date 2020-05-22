#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <list>
#include <memory>
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
	constexpr char ARG_VISUAL[] = "--visual";
	
	constexpr unsigned int ISO_TIME_BUFFER_SIZE{255};
}

float getImageBusiness(const cv::Mat& mask);
std::list<std::string> persistImages(Persistence& persistence, std::list<cv::Mat*> images, char isoTime[ISO_TIME_BUFFER_SIZE], const std::time_t& triggerTime);
void sendMail(const std::string& email, char isoTime[ISO_TIME_BUFFER_SIZE], const std::time_t& triggerTime, const float saturation, const Persistence& storage, std::list<std::string> attachmentFiles);


int main(int argc, char** argv)
{
	std::string url;
	std::string email;
	std::string storage;
	bool visual = false;
	
	// parse command line arguments
	ArgumentParser parser;
	parser.registerArgument(ARG_URL, "rtsp://user:password@localhost:554/stream");
	parser.registerArgument(ARG_EMAIL, "my@address.com");
	parser.registerArgument(ARG_STORAGE, ".");
	parser.registerArgument(ARG_VISUAL, "True");
	parser.parse(argc, argv);
	
	auto argUrl = parser[ARG_URL];
	auto argEmail = parser[ARG_EMAIL];
	auto argStorage = parser[ARG_STORAGE];
	auto argVisual= parser[ARG_VISUAL];
	
	if (argUrl.exists) url = argUrl.value;
	if (argEmail.exists) email = argEmail.value;
	if (argStorage.exists) storage = argStorage.value;
	if (argVisual.exists && ((argVisual.value == "True") || (argVisual.value == "true"))) visual = true;
	
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
	
	if(visual)
	{
		cv::namedWindow("Surveillance", cv::WINDOW_AUTOSIZE);
		cv::namedWindow("Foreground", cv::WINDOW_AUTOSIZE);
		cv::namedWindow("ForegroundSmoothed", cv::WINDOW_AUTOSIZE);	
	}
	
    cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorMOG2(FPS * 60 * 5, 32, true);
    //cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorKNN(FPS * 60 * 5, 100, true);
    if (!backgroundSubtractor)
    {
		std::cerr << "Could not create backgound subtractor..." << std::endl;
		return -1;    
    }
	std::chrono::system_clock::time_point trigger = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point currentTime;
	
	
	// Set location for videos & images to be stored.
	Persistence persistence(storage);
	bool running{true};	
	char isoTime[ISO_TIME_BUFFER_SIZE] = {0};		
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
		if (visual) cv::imshow("Foreground", foregroundMask);
		
		// Reduce Noice on forground mask
        cv::erode(foregroundMask, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
        cv::dilate(foregroundMaskSmoothed, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
		if (visual) cv::imshow("Surveillance", frameSmoothed);
		if (visual) cv::imshow("ForegroundSmoothed", foregroundMaskSmoothed);
		
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
				
			// Save triggering images fullfilling saturation condition.
			std::list<cv::Mat*> imageList{&frame, &foregroundMask};
			std::list<std::string> persistedImages = persistImages(persistence, imageList, isoTime, cTrigger);
			
			//send mail
			sendMail(email, isoTime, cTrigger, saturation, persistence, persistedImages);
	
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
	if (visual)
	{
		cv::destroyAllWindows();
	}
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

std::list<std::string> persistImages(Persistence& persistence, std::list<cv::Mat*> images, char isoTime[ISO_TIME_BUFFER_SIZE], const std::time_t& triggerTime)
{
	std::list<std::string> persisted{};
	strftime(isoTime, ISO_TIME_BUFFER_SIZE,"%FT%H-%M-%S", std::localtime(&triggerTime));
	
	int index = 1;
	for (auto& image: images)
	{
		std::string jpg = std::string(isoTime) + '_' + std::to_string(index) + std::string(".jpg");	
		if (persistence.saveJpg(*image, jpg)) 
		{
			persisted.push_back(jpg);
		}
		++index;
	
	}
	return persisted;
}

void sendMail(const std::string& email, char isoTime[ISO_TIME_BUFFER_SIZE], const std::time_t& triggerTime, const float saturation, const Persistence& storage, std::list<std::string> attachmentFiles)
{	
	// Set from, to
	EMailNotifier notifier(email, email);
	std::stringstream compose;	

	// Subject, currently colons are erronous during image creation.
	compose << "Intrusion detected " << isoTime;
	std::string subject = compose.str();
	
	// Body
	strftime(isoTime, ISO_TIME_BUFFER_SIZE,"%FT%H:%M:%S", std::localtime(&triggerTime));
	compose.str("");
	compose << "Possible Intrusion detected (" << isoTime << ") with an image saturation of " 
			<< saturation * 100.0 << "%. "
			<< "Please see attachment for detected image area.";
	std::string body = compose.str();
	std::cout << body << std::endl;				
	
	// Attachment
	
	// Send EMail
	notifier.alert(subject, body, storage.getLocation(), attachmentFiles);
}

