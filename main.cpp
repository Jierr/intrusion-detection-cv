#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sstream>

#include <opencv2/video/video.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

using namespace std;
using namespace cv;

namespace
{
	constexpr int FPS(24);
	constexpr int MS_PER_FRAME (1000/FPS);
	constexpr int VK_ESCAPE{27};
	constexpr auto TRIGGER_DELAY_SECONDS = std::chrono::seconds(120);
	constexpr float SATURATION_TRIGGER = 0.035f;
	
	constexpr char ARG_URL[] = "--url";
	constexpr char ARG_EMAIL[] = "--email";
	constexpr char ARG_EPASSWORD[] = "--epassword";
}

float getImageBusiness(const cv::Mat& mask);

class EMailNotifier
{
public:
	EMailNotifier(const std::string& from, const std::string& to, const std::string& password);
	virtual ~EMailNotifier();
	void alert(std::string subject, std::string body, std::string attachment);
private:
	std::string mFrom;
	std::string mTo;
	std::string mPassword;
};

EMailNotifier::EMailNotifier(const std::string& from, const std::string& to, const std::string& password)
{
	mFrom = from;
	mTo = to;
	mPassword = password;
	
}

EMailNotifier::~EMailNotifier()
{

}

void EMailNotifier::alert(std::string subject, std::string body, std::string attachment)
{
	std::stringstream command;
	auto time = std::chrono::system_clock::now();
	std::time_t cTime = std::chrono::system_clock::to_time_t(time);
	command << "echo \"Subject: " << subject
			<< "\\" << std::endl << std::endl 
			<< body
			<< "\" | sendmail -v " << mTo << "&";
			
	system(command.str().c_str());
}


class ArgumentParser
{
public:
	struct Argument
	{	
		bool exists;
		std::string name;
		std::string value;
	};
	
	ArgumentParser() = default;
	virtual ~ArgumentParser() = default;
	void parse(int argc, char** argv);
	void registerArgument(const std::string& parameter, const std::string& value);
	Argument operator[](std::string name);
private:
	std::map<std::string, std::string> mArgument;
};

void ArgumentParser::parse(int argc, char** argv)
{
	for(int arg = 1; arg < argc; ++arg)
	{
		std::string name = argv[arg];
		auto argument = mArgument.find(name);
		if ((argument != mArgument.end()) && (arg < argc-1))
		{
			argument->second = std::string(argv[arg+1]);
		}
	}
}

void ArgumentParser::registerArgument(const std::string& parameter, const std::string& value)
{
	mArgument[parameter] = value;
}

ArgumentParser::Argument ArgumentParser::operator[](std::string name)
{
	auto argument = mArgument.find(name);
	if(argument != mArgument.end())
	{
		return {true, argument->first, argument->second};
	}
	
	return {false, "",""};
}



int main(int argc, char** argv)
{
	std::string url;
	std::string email;
	std::string password;
	
	ArgumentParser parser;
	parser.registerArgument(ARG_URL, "rtsp://user:password@localhost:554/stream");
	parser.registerArgument(ARG_EMAIL, "my@address.com");
	parser.registerArgument(ARG_EPASSWORD, "");
	parser.parse(argc, argv);
	
	auto argUrl = parser[ARG_URL];
	auto argEmail = parser[ARG_EMAIL];
	auto argEPassword = parser[ARG_EPASSWORD];
	
	if (argUrl.exists) url = argUrl.value;
	if (argEmail.exists) email = argEmail.value;
	if (argEPassword.exists) password = argEPassword.value;
	
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
	cv::namedWindow("Foreground2", cv::WINDOW_AUTOSIZE);
	
    cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorMOG2(FPS * 60 * 5, 32, true);
    //cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorKNN(FPS * 60 * 5, 100, true);
    if (!backgroundSubtractor)
    {
		std::cerr << "Could not create backgound subtractor..." << std::endl;
		return -1;    
    }
	cv::Mat frame, foregroundMask;
	bool running{true};
	std::chrono::system_clock::time_point trigger = std::chrono::system_clock::time_point::min();
	std::chrono::system_clock::time_point currentTime;
	
	EMailNotifier notifier(email, email, password);
	while(running) 
	{
		if (!capture.read(frame))
		{
			std::cerr << "Could not read frame..." << std::endl;
			continue;
		}
		
		cv::blur(frame, frame, cv::Size(3,3));
        backgroundSubtractor->apply(frame, foregroundMask);
        
		cv::imshow("Foreground1", foregroundMask);
        cv::erode(foregroundMask, foregroundMask, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
        cv::dilate(foregroundMask, foregroundMask, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)));
		cv::imshow("Surveillance", frame);
		cv::imshow("Foreground2", foregroundMask);
		float saturation = getImageBusiness(foregroundMask);
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

