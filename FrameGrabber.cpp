/*
 * FrameGrabber.cpp
 *
 *  Created on: 17.06.2020
 *      Author: jierr
 */

#include "FrameGrabber.hpp"

#include <opencv2/videoio/videoio.hpp>

FrameGrabber::FrameGrabber(unsigned int maxErrors)
        :
        mMaxErrors(maxErrors), mRunning(false), mTerminated(true), mHasNewFrame(false), mFramebufferIndex(0)
{

}

FrameGrabber::~FrameGrabber()
{
    stop();
}

void FrameGrabber::start(const std::string &url)
{
    stop();
    if (mThread == nullptr)
    {
        mTerminated = false;
        mRunning = true;
        mHasNewFrame = false;
        mFramebufferIndex = true;
        mThread.reset(new std::thread(&FrameGrabber::run, this, url));
    }
}

void FrameGrabber::stop()
{
    if (mThread != nullptr)
    {
        mRunning = false;
        if (mThread->joinable())
            mThread->join();
        mThread.reset(nullptr);
    }
}

void FrameGrabber::run(const std::string url)
{
    cv::VideoCapture camera;
    unsigned int errors = 0;

    if (url.empty())
    {
        // Use default (Webcam)
        camera.open(0);
    }
    else
    {
        camera.open(url);
    }

    if (!camera.isOpened())
    {
        mRunning = false;
        mTerminated = true;
        return;
    }

    while (mRunning && camera.isOpened() && (errors < mMaxErrors))
    {
        mLock.lock();
        int last = mFramebufferIndex;
        mFramebufferIndex = (mFramebufferIndex + 1) % 2;
        if (!camera.read(mFramebuffer[mFramebufferIndex]))
        {
            mFramebufferIndex = last;
            mLock.unlock();
            ++errors;
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        else
        {
            mHasNewFrame = true;
            mLock.unlock();
            errors = 0;
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (camera.isOpened())
    {
        camera.release();
    }

    mTerminated = true;
}

bool FrameGrabber::terminated()
{
    if (mThread == nullptr)
    {
        return true;
    }
    return mTerminated;
}

bool FrameGrabber::get(cv::Mat &frame)
{
    if (mThread == nullptr)
    {
        return false;
    }

    mLock.lock();
    if (mHasNewFrame)
    {
        frame = mFramebuffer[mFramebufferIndex];
        mHasNewFrame = false;
        mLock.unlock();
        return true;
    }
    mLock.unlock();
    return false;
}
