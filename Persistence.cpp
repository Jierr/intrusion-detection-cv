#include "Persistence.hpp"

#include <iostream>
#include <vector>

Persistence::Persistence(const std::string& location)
: mLocation(location)
{
}

bool Persistence::saveJpg(const cv::Mat& frame, std::string name, unsigned int quality)
{
    std::vector<int> compression;
    compression.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression.push_back((quality>100)?100:quality);
    
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

const std::string& Persistence::getLocation() const
{
	return mLocation;
}
