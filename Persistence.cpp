#include "Persistence.hpp"

#include <iostream>
#include <vector>

Persistence::Persistence(const std::string& location)
: mLocation(location)
{

}

Persistence::~Persistence()
{
}

bool Persistence::saveJpg(const cv::Mat& frame, std::string name)
{
    std::vector<int> compression;
    compression.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression.push_back(100);
    
	std::string filePath = mLocation + '/' + name;
    try 
    {
		cv::imwrite(filePath, frame, compression);
    }
    catch (std::runtime_error& ex) 
    {
    	std::cerr << "Exception converting JPG image: " << ex.what() << std::endl;
        return false;	
 	}
 	return true;
}
