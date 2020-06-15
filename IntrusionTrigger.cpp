#include "IntrusionTrigger.hpp"

#include <cmath>
#include <iostream>
#include <string>

IntrusionTrigger::IntrusionTrigger(unsigned int triggerFrames, double saturationTrigger)
{
    reset(triggerFrames, saturationTrigger);
}

void IntrusionTrigger::reset(unsigned int triggerFrames, double saturationTrigger)
{
    mTriggerFrames = triggerFrames;
    mSaturationTrigger = saturationTrigger;
    mTriggerCondition = 0.0;

    /* [(1 - d^(n)) / (1 - d) / d^n] = [(1 + (1 + (1 + ... ) / d) / d) / d] */
    mTriggerIndicator = (1.0 - std::pow(mDegradation, static_cast<double>(mTriggerFrames))) / (1.0 - mDegradation);
    mTriggerIndicator /= std::pow(mDegradation, static_cast<double>(mTriggerFrames));
    std::cout << "IntrusionTrigger -> Triggering for values greater or equal to " << mTriggerIndicator << std::endl;
}

bool IntrusionTrigger::hasTriggered()
{
    return mTriggerCondition > (mTriggerIndicator - mTriggerIndicatorTolerance);
}

double IntrusionTrigger::update(const cv::Mat &foreground)
{
    const int maxSaturation = foreground.rows * foreground.cols;
    int saturation = 0;
    for (int i = 0; i < foreground.rows; i++)
    {
        for (int j = 0; j < foreground.cols; j++)
        {
            if (foreground.at<unsigned char>(i, j) != 0)
            {
                ++saturation;
            }
        }
    }

    double trigger = static_cast<double>(saturation) / static_cast<double>(maxSaturation);
    double triggerWeight = 0.0;
    if (trigger >= mSaturationTrigger)
    {
        triggerWeight = 1.0;
    }

    mTriggerCondition = (triggerWeight + mTriggerCondition) / mDegradation;
#ifdef _DEBUG
	if (trigger >= mSaturationTrigger)
	{
		std::cout << "Trigger after " << mTriggerFrames << " Frames with Indicator at " << mTriggerIndicator << std::endl
			        << mTriggerCondition << " > " << mTriggerIndicator << " ?" << std::endl;
	}
#endif
    return trigger;
}
