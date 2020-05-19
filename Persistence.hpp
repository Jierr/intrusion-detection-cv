#ifndef _PERSISTENCE_HPP_
#define _PERSISTENCE_HPP_

#include <opencv2/opencv.hpp>
#include <string>

class Persistence
{
public:
	explicit Persistence(const std::string& location = ".");
	~Persistence();
	bool saveJpg(const cv::Mat& frame, std::string name);
private:
	std::string mLocation;
};

#endif //_PERSISTENCE_HPP_
