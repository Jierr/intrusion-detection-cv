/*
 * FrameGrabber.hpp
 *
 *  Created on: 17.06.2020
 *      Author: jierr
 */

#ifndef FRAMEGRABBER_HPP_
#define FRAMEGRABBER_HPP_

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <opencv2/core/mat.hpp>
#include <string>
#include <thread>
#include <unistd.h>

/**
 * @brief The FrameGrabber asynchronously fetches frame from a rtsp camera stream
 */
class FrameGrabber
{
public:
    /**
     * @brief CTOR
     * @param maxErrors Number of consecutive frame read errors after which the grabber thread terminates
     */
    explicit FrameGrabber(unsigned int maxErrors);

    /**
     * @brief DTOR
     *
     * Terminate a running thread and cleanup.
     */
    virtual ~FrameGrabber();

    /**
     * @brief Start the thread to grab frames.
     */
    void start(const std::string &url);

    /**
     * @brief Stop the frame grabbing thread.
     */
    void stop();

    /**
     * @brief Check if thread terminated
     * @retval true frame grabber thread terminated
     * @retval false frame grabber thread still running
     */
    bool terminated();

    /**
     * @brief get the latest frame
     * @param frame The buffer in which the latest frame is copied to.
     * @return true, when a frame is available; false otherwise
     */
    bool get(cv::Mat &frame);

protected:
    /**
     * @brief Thread Execution instance.
     */
    void run(const std::string url);

private:
    const unsigned int mMaxErrors;
    std::unique_ptr<std::thread> mThread;
    std::atomic<bool> mRunning;
    std::atomic<bool> mTerminated;
    std::atomic<bool> mHasNewFrame;
    std::mutex mLock;
    std::array<cv::Mat, 2> mFramebuffer;
    std::atomic<int> mFramebufferIndex;
};

#endif /* FRAMEGRABBER_HPP_ */
