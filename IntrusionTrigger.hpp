#ifndef _INTRUSIONTRIGGER_HPP_
#define _INTRUSIONTRIGGER_HPP_

#include <opencv2/opencv.hpp>

class IntrusionTrigger
{
public:
	IntrusionTrigger(unsigned int triggerFrames = 12, double saturationTrigger = 0.02);
	virtual ~IntrusionTrigger() = default;
	bool hasTriggered();
	double update(const cv::Mat& foreground);
	void reset(unsigned int triggerFrames = 12, double saturationTrigger = 0.02);
private:
	unsigned int mTriggerFrames;
	double mSaturationTrigger;
	double mTriggerIndicator;
	double mTriggerCondition;
	const double mTriggerIndicatorTolerance{0.0001};
	const double mDegradation{1.01};
};

#endif //_INTRUSIONTRIGGER_HPP_
