#include <chrono>
#include <cmath>
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
#include "IntrusionTrigger.hpp"
#include "Persistence.hpp"

using namespace std;
using namespace cv;

/*
 * Todo: Record a video, on trigger, store and/or send via mail.
 * Use high quality image for Email
 * Use seperate Email service.
 */

namespace
{
	constexpr int FPS{14};
	constexpr int MS_PER_FRAME{1000/FPS};
	constexpr int VK_ESCAPE{27};
	constexpr auto TRIGGER_DELAY_SECONDS = std::chrono::seconds(90);
	constexpr double TRIGGER_SATURATION{0.020};
	constexpr unsigned int MINOR_TRIGGER_ON_CONSECUTIVE_FRAME{FPS*3};
	constexpr unsigned int MAJOR_TRIGGER_ON_CONSECUTIVE_FRAME{FPS*10};
	
	
	constexpr char ARG_URL[] = "--url";
	constexpr char ARG_EMAIL[] = "--email";
	constexpr char ARG_STORAGE[] = "--storage";
	constexpr char ARG_VISUAL[] = "--visual";
	constexpr char ARG_SCRIPTS[] = "--scripts";
	
	constexpr unsigned int ISO_TIME_BUFFER_SIZE{255};
	constexpr unsigned int CAPTURE_ERROR_REINITIALIZE{100};
}

enum AlertLevel
{
	LOW = 0,
	MIDDLE,
	HIGH
};

std::list<std::string> persistImages(Persistence& persistence, std::list<cv::Mat*> images, char isoTime[ISO_TIME_BUFFER_SIZE], const std::time_t& triggerTime);
void sendMail(const AlertLevel level, const std::string& script, const std::string& email, char isoTime[ISO_TIME_BUFFER_SIZE], 
		const std::time_t& triggerTime, const float saturation, float fps, 
		const Persistence& storage, std::list<std::string> attachmentFiles);

int main(int argc, char** argv)
{
	std::string url;
	std::string email;
	std::string storage;
	std::string scripts;
	bool visual = false;
	
	// parse command line arguments
	ArgumentParser parser;
	parser.registerArgument(ARG_URL, "rtsp://user:password@localhost:554/stream");
	parser.registerArgument(ARG_EMAIL, "my@address.com");
	parser.registerArgument(ARG_STORAGE, ".");
	parser.registerArgument(ARG_SCRIPTS, ".");
	parser.registerArgument(ARG_VISUAL, "False");
	parser.parse(argc, argv);
	
	auto argUrl = parser[ARG_URL];
	auto argEmail = parser[ARG_EMAIL];
	auto argStorage = parser[ARG_STORAGE];
	auto argScripts= parser[ARG_SCRIPTS];
	auto argVisual= parser[ARG_VISUAL];
	
	if (argUrl.exists) url = argUrl.value;
	if (argEmail.exists) email = argEmail.value;
	if (argStorage.exists) storage = argStorage.value;
	if (argScripts.exists) scripts = argScripts.value;
	if (argVisual.exists && ((argVisual.value == "True") || (argVisual.value == "true"))) visual = true;
	
	std::cout << ARG_URL << " = " << url << std::endl;
	std::cout << ARG_EMAIL << " = " << email << std::endl;
	std::cout << ARG_STORAGE << " = " << storage << std::endl;
	std::cout << ARG_SCRIPTS << " = " << scripts << std::endl;
	
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
	
  cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorMOG2(FPS * 60 * 2, 32, true);
  //cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorKNN(FPS * 60 * 5, 100, true);
  if (!backgroundSubtractor)
  {
    std::cerr << "Could not create backgound subtractor..." << std::endl;
    return -1;    
  }
	std::chrono::system_clock::time_point minorTrigger = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point majorTrigger = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point currentTime;
	std::chrono::system_clock::time_point fpsTimer = std::chrono::system_clock::now();
	
	
	// Set location for videos & images to be stored.
	Persistence persistence(storage);
	IntrusionTrigger minorIntrusionTrigger(MINOR_TRIGGER_ON_CONSECUTIVE_FRAME, TRIGGER_SATURATION);
	IntrusionTrigger majorIntrusionTrigger(MAJOR_TRIGGER_ON_CONSECUTIVE_FRAME, TRIGGER_SATURATION);
	bool running{true};	
	char isoTime[ISO_TIME_BUFFER_SIZE] = {0};		
	cv::Mat frame, frameSmoothed, foregroundMask, foregroundMaskSmoothed;
	unsigned int captureErrors{0};
	unsigned int frames{0};
	float fps = 0.0f;
	while(running) 
	{
		if(captureErrors > CAPTURE_ERROR_REINITIALIZE)
		{
			capture.release();
			if (visual)
			{
				cv::destroyAllWindows();
			}
			return -1;
		}
		
		try
		{		
			if (!capture.read(frame))
			{
				std::cerr << "Could not read frame..." << std::endl;
				++captureErrors;
				continue;
			}		
		}    
		catch (std::runtime_error& ex) 
		{
			std::cerr << "Exception acquiring next frame: " << ex.what() << std::endl;
			++captureErrors;
		    continue;	
	 	}
	 	
		// Create Background from noise reduced image
		cv::blur(frame, frameSmoothed, cv::Size(3,3));
		// Remove light sensitive reactions
		cv::cvtColor(frameSmoothed, frameSmoothed, cv::COLOR_RGB2YUV);
		std::vector<cv::Mat> channels;
		cv::split(frameSmoothed, channels);
		cv::equalizeHist(channels[0], channels[0]);
		cv::merge(channels, frameSmoothed);
		cv::cvtColor(frameSmoothed, frameSmoothed, CV_YUV2RGB);
		
    backgroundSubtractor->apply(frameSmoothed, foregroundMask);
    if (visual) cv::imshow("Foreground", foregroundMask);

    // Reduce Noice on forground mask
    cv::erode(foregroundMask, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
    cv::dilate(foregroundMaskSmoothed, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
    if (visual) cv::imshow("Surveillance", frameSmoothed);
    if (visual) cv::imshow("ForegroundSmoothed", foregroundMaskSmoothed);

    // Evaluate image foreground saturation 
    double minorSaturation = minorIntrusionTrigger.update(foregroundMaskSmoothed);
    double majorSaturation = majorIntrusionTrigger.update(foregroundMaskSmoothed);

    currentTime = std::chrono::system_clock::now();

		// Calculate Framerate
		if(currentTime > (fpsTimer + std::chrono::seconds(10)))
		{
			auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - fpsTimer);
			fps = static_cast<float>(frames) / milliseconds.count() * 1000.0;
			fpsTimer = currentTime;
			frames = 0;
			std::cout << "FPS: " << fps << std::endl;
		}
		++frames;

		// check, if movement was detected
				
		if (majorIntrusionTrigger.hasTriggered() && (currentTime > majorTrigger + TRIGGER_DELAY_SECONDS))
		{
			// Reset timer for next trigger condition
			majorTrigger = std::chrono::system_clock::now();
			std::time_t cTrigger = std::chrono::system_clock::to_time_t(majorTrigger);
			std::cout << "Possible Intrusion: " << std::ctime(&cTrigger) << std::endl;								
				
			// Save triggering images fullfilling saturation condition.
			std::list<cv::Mat*> imageList{&frame, &foregroundMask};
			std::list<std::string> persistedImages = persistImages(persistence, imageList, isoTime, cTrigger);
			
			//send mail
			sendMail(AlertLevel::HIGH, scripts, email, isoTime, cTrigger, majorSaturation, fps, persistence, persistedImages);	
		}
		else if (minorIntrusionTrigger.hasTriggered() && (currentTime > minorTrigger + TRIGGER_DELAY_SECONDS))
		{
			// Reset timer for next trigger condition
			minorTrigger = std::chrono::system_clock::now();
			std::time_t cTrigger = std::chrono::system_clock::to_time_t(minorTrigger);
			std::cout << "Possible Intrusion: " << std::ctime(&cTrigger) << std::endl;								
				
			// Save triggering images fullfilling saturation condition.
			std::list<cv::Mat*> imageList{&frame, &foregroundMask};
			std::list<std::string> persistedImages = persistImages(persistence, imageList, isoTime, cTrigger);
			
			//send mail
			sendMail(AlertLevel::LOW, scripts, email, isoTime, cTrigger, minorSaturation, fps, persistence, persistedImages);	
		}

		
		if (visual)
		{
			// Wait and Evaluate pressed keys.
			int key = cv::waitKey(1);
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

	}
	
	// Cleanup
	capture.release();
	if (visual)
	{
		cv::destroyAllWindows();
	}
	return 0;
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

void sendMail(const AlertLevel level, const std::string& script, const std::string& email, char isoTime[ISO_TIME_BUFFER_SIZE], 
		const std::time_t& triggerTime, const float saturation, float fps, 
		const Persistence& storage, std::list<std::string> attachmentFiles)
{	
	// Set from, to
	EMailNotifier notifier(email, email, script);
	std::stringstream compose;	

	// Subject, currently colons are erronous during image creation.
	if (level == AlertLevel::HIGH)
	{	
		compose << "HIGH Alert: Intrusion detected " << isoTime;
	}
	else
	{
		compose << "LOW Alert: Disturbtion detected " << isoTime;	
	}
	std::string subject = compose.str();
	
	// Body
	strftime(isoTime, ISO_TIME_BUFFER_SIZE,"%FT%H:%M:%S", std::localtime(&triggerTime));
	compose.str("");
	compose << "Possible Intrusion detected (" << isoTime << ") with an image saturation of " 
			<< saturation * 100.0 << "%. "
			<< "Please see attachment for detected image area." << std::endl
			<< "Current FPS are " << fps << ".";
	std::string body = compose.str();
	std::cout << body << std::endl;				
		
	// Send EMail
	notifier.alert(subject, body, storage.getLocation(), attachmentFiles);
}

