#ifndef _PERSISTENCE_HPP_
#define _PERSISTENCE_HPP_

#include <opencv2/opencv.hpp>
#include <string>

class Persistence
{
public:
    explicit Persistence(const std::string &location = ".");
    virtual ~Persistence() = default;
    bool saveJpg(const cv::Mat &frame, std::string name, unsigned int quality = 75);
    const std::string& getLocation() const;
private:
    std::string mLocation;
};

#endif //_PERSISTENCE_HPP_
