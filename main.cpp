#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <functional>

#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <string>

// daemon
#include <atomic>
#include <mutex>

//Frame Grabber
#include <thread>
#include <unistd.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/videoio/videoio.hpp>

#include "ArgumentParser.hpp"
#include "Daemon.hpp"
#include "EMailNotifier.hpp"
#include "FrameGrabber.hpp"
#include "IntrusionTrigger.hpp"
#include "Persistence.hpp"
#include "Utility.hpp"

using namespace std;
using namespace cv;

/*
 * Todo: Record a video, on trigger, store and/or send via mail.
 * Use high quality image for Email
 * Use seperate Email service / Thread. Use lib ssmtp
 * Mark images as sent or pending. On service start check, if there are pending
 * images.
 * Log into file
 * Add Framerate control
 */

namespace
{
constexpr char DAEMON_NAME[] = "intrusion-detection";

constexpr int FPS { 10 };
constexpr int VK_ESCAPE { 27 };
constexpr auto TRIGGER_DELAY_SECONDS = std::chrono::seconds(90);
constexpr double TRIGGER_SATURATION { 0.030 };
constexpr unsigned int MINOR_TRIGGER_ON_CONSECUTIVE_FRAME { FPS * 15 };
constexpr unsigned int MAJOR_TRIGGER_ON_CONSECUTIVE_FRAME { FPS * 20 };

constexpr char ARG_FOREGROUND[] = "--foreground";
constexpr char ARG_RUNDIR[] = "--run-dir";

constexpr char ARG_URL[] = "--url";
constexpr char ARG_EMAIL[] = "--email";
constexpr char ARG_STORAGE[] = "--storage";
constexpr char ARG_VISUAL[] = "--visual";
constexpr char ARG_SCRIPTS[] = "--scripts";

constexpr unsigned int ISO_TIME_BUFFER_SIZE { 255 };
constexpr unsigned int CAPTURE_ERROR_REINITIALIZE { 10000 };

}  // namespace

enum AlertLevel
{
    LOW = 0,
    MIDDLE,
    HIGH
};

std::list<std::string>
persistImages(Persistence &persistence, std::list<cv::Mat*> images, char isoTime[ISO_TIME_BUFFER_SIZE],
        const std::time_t &triggerTime);
void
sendMail(const AlertLevel level, EMailNotifier &notifier, char isoTime[ISO_TIME_BUFFER_SIZE],
        const std::time_t &triggerTime, const float saturation, float fps, const Persistence &storage,
        std::list<std::string> attachmentFiles);

class IntrusionDetectionDaemon: public Daemon
{
public:
    explicit IntrusionDetectionDaemon(const std::string &name);
    ~IntrusionDetectionDaemon();
    int run(int argc, char **argv) override;
    void tearDown();

    static std::shared_ptr<IntrusionDetectionDaemon> getInstance(const std::string &name);
private:
    __sighandler_t getSignalHandler() override;
    static void signalHandler(int number);
    static std::mutex mLock;
    static std::shared_ptr<IntrusionDetectionDaemon> mDaemon;
    static std::atomic<bool> mRunning;

    ArgumentParser mParser;
};


IntrusionDetectionDaemon::IntrusionDetectionDaemon(const std::string &name)
        :
        Daemon(name)
{
    mRunning = true;
    mParser.registerArgument(ARG_URL, "");
    mParser.registerArgument(ARG_EMAIL, "my@address.com");
    mParser.registerArgument(ARG_STORAGE, ".");
    mParser.registerArgument(ARG_SCRIPTS, ".");
    mParser.registerArgument(ARG_VISUAL, "False");
}

IntrusionDetectionDaemon::~IntrusionDetectionDaemon()
{
    tearDown();
}

int main(int argc, char **argv)
{
    ArgumentParser parser;
    bool foreground { true };
    std::string runDir = "/tmp/";
    parser.registerArgument(ARG_FOREGROUND, "true");
    parser.registerArgument(ARG_RUNDIR, "/tmp/");
    parser.parse(argc, argv);

    if (parser[ARG_FOREGROUND].exists)
        foreground = (Utility::toLower(parser[ARG_FOREGROUND].value) == "true");
    if (parser[ARG_RUNDIR].exists)
        runDir = parser[ARG_RUNDIR].value;

    std::shared_ptr<IntrusionDetectionDaemon> daemon(IntrusionDetectionDaemon::getInstance(DAEMON_NAME));
    std::cout << "Starting Daemon...";

    if (foreground)
    {
        std::cout << " in foreground." << std::endl;
        return daemon->run(argc, argv);
    }
    else
    {
        std::cout << " in background." << std::endl;
        daemon->openLog();
        daemon->setRunDir(runDir);
        if (daemon->daemonize())
        {
            return daemon->run(argc, argv);
        }
        else
        {
            std::cerr << " failed.";
            return -1;
        }
    }
    return 0;
}

std::mutex IntrusionDetectionDaemon::mLock;
std::shared_ptr<IntrusionDetectionDaemon> IntrusionDetectionDaemon::mDaemon;
std::atomic<bool> IntrusionDetectionDaemon::mRunning;

__sighandler_t IntrusionDetectionDaemon::getSignalHandler()
{
    return signalHandler;
}

void IntrusionDetectionDaemon::signalHandler(int sig)
{
    std::cout << "Interrupt signal (" << sig << ") received.\n";
    switch (sig)
    {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        mRunning = false;
        break;
    default:
        break;
    }
}

std::shared_ptr<IntrusionDetectionDaemon> IntrusionDetectionDaemon::getInstance(const std::string &name)
{
    mLock.lock();
    if (!mDaemon)
    {
        mRunning = true;
        mDaemon = std::shared_ptr<IntrusionDetectionDaemon>(new IntrusionDetectionDaemon(name));
    }
    mLock.unlock();
    return mDaemon;
}

void IntrusionDetectionDaemon::tearDown()
{
}

int IntrusionDetectionDaemon::run(int argc, char **argv)
{
    std::string url;
    std::string email;
    std::string storage;
    std::string scripts;
    bool visual = false;

    // parse command line arguments

    mParser.parse(argc, argv);

    auto argUrl = mParser[ARG_URL];
    auto argEmail = mParser[ARG_EMAIL];
    auto argStorage = mParser[ARG_STORAGE];
    auto argScripts = mParser[ARG_SCRIPTS];
    auto argVisual = mParser[ARG_VISUAL];

    if (argUrl.exists)
        url = argUrl.value;
    if (argEmail.exists)
        email = argEmail.value;
    if (argStorage.exists)
        storage = argStorage.value;
    if (argScripts.exists)
        scripts = argScripts.value;
    if (argVisual.exists && ((argVisual.value == "True") || (argVisual.value == "true")))
        visual = true;

    std::cout << ARG_URL << " = " << url << std::endl;
    std::cout << ARG_EMAIL << " = " << email << std::endl;
    std::cout << ARG_STORAGE << " = " << storage << std::endl;
    std::cout << ARG_SCRIPTS << " = " << scripts << std::endl;

    if (visual)
    {
        cv::namedWindow("Surveillance", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("Foreground", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("ForegroundSmoothed", cv::WINDOW_AUTOSIZE);
    }

    cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor = createBackgroundSubtractorMOG2(FPS * 60 * 2, 32, true);
    // cv::Ptr<cv::BackgroundSubtractor> backgroundSubtractor =
    // createBackgroundSubtractorKNN(FPS * 60 * 5, 100, true);
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

    FrameGrabber capture(CAPTURE_ERROR_REINITIALIZE);
    EMailNotifier notifier(email, email, scripts);
    Persistence persistence(storage);
    IntrusionTrigger minorIntrusionTrigger(MINOR_TRIGGER_ON_CONSECUTIVE_FRAME, TRIGGER_SATURATION);
    IntrusionTrigger majorIntrusionTrigger(MAJOR_TRIGGER_ON_CONSECUTIVE_FRAME, TRIGGER_SATURATION);
    char isoTime[ISO_TIME_BUFFER_SIZE] = { 0 };
    cv::Mat frame, frameSmoothed, foregroundMask, foregroundMaskSmoothed;
    unsigned int frames { 0 };
    float fps = 0.0f;
    while (mRunning)
    {
        if (capture.terminated())
        {
            std::cerr << "Frame Grabber is not running, restarting..." << std::endl;
            capture.start(url);
        }

        if (!capture.get(frame))
            continue;
        // Create Background from noise reduced image
        cv::blur(frame, frameSmoothed, cv::Size(3, 3));
        // Remove light sensitive reactions
        cv::cvtColor(frameSmoothed, frameSmoothed, cv::COLOR_RGB2YUV);
        std::vector<cv::Mat> channels;
        cv::split(frameSmoothed, channels);
        cv::equalizeHist(channels[0], channels[0]);
        cv::merge(channels, frameSmoothed);
        cv::cvtColor(frameSmoothed, frameSmoothed, CV_YUV2RGB);

        backgroundSubtractor->apply(frameSmoothed, foregroundMask);
        if (visual)
            cv::imshow("Foreground", foregroundMask);

        // Reduce Noice on forground mask
        cv::erode(foregroundMask, foregroundMaskSmoothed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));
        cv::dilate(foregroundMaskSmoothed, foregroundMaskSmoothed,
                cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));
        if (visual)
            cv::imshow("Surveillance", frameSmoothed);
        if (visual)
            cv::imshow("ForegroundSmoothed", foregroundMaskSmoothed);

        // Evaluate image foreground saturation
        double minorSaturation = minorIntrusionTrigger.update(foregroundMaskSmoothed);
        double majorSaturation = majorIntrusionTrigger.update(foregroundMaskSmoothed);

        currentTime = std::chrono::system_clock::now();

        // Calculate Framerate
        if (currentTime > (fpsTimer + std::chrono::seconds(10)))
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

            // Save triggering images fullfilling saturation condition.
            std::list<cv::Mat*> imageList { &frame, &foregroundMask };
            std::list<std::string> persistedImages = persistImages(persistence, imageList, isoTime, cTrigger);

            // send mail
            sendMail(AlertLevel::HIGH, notifier, isoTime, cTrigger, majorSaturation, fps, persistence, persistedImages);
        }
        else if (minorIntrusionTrigger.hasTriggered() && (currentTime > minorTrigger + TRIGGER_DELAY_SECONDS))
        {
            // Reset timer for next trigger condition
            minorTrigger = std::chrono::system_clock::now();
            std::time_t cTrigger = std::chrono::system_clock::to_time_t(minorTrigger);

            // Save triggering images fullfilling saturation condition.
            std::list<cv::Mat*> imageList { &frame, &foregroundMask };
            std::list<std::string> persistedImages = persistImages(persistence, imageList, isoTime, cTrigger);

            // send mail
            sendMail(AlertLevel::LOW, notifier, isoTime, cTrigger, minorSaturation, fps, persistence, persistedImages);
        }
        if (notifier.check() == EMailNotifier::EMailSendStatus::Error)
        {
            std::cerr << "Sending the last Email failed!";
        }
        else if (notifier.check() == EMailNotifier::EMailSendStatus::Success)
        {
            std::cout << "Sending the last Email succeeded!";
        }

        if (visual)
        {
            // Wait and Evaluate pressed keys.
            int key = cv::waitKey(1);
            switch (key)
            {
            case VK_ESCAPE:
                std::cout << "Terminating..." << std::endl;
                mRunning = false;
                break;
            default:
                break;
            }
        }
    }

    // Cleanup
    if (visual)
    {
        cv::destroyAllWindows();
    }

    return 0;
}

std::list<std::string> persistImages(Persistence &persistence, std::list<cv::Mat*> images,
        char isoTime[ISO_TIME_BUFFER_SIZE], const std::time_t &triggerTime)
{
    std::list<std::string> persisted { };
    strftime(isoTime, ISO_TIME_BUFFER_SIZE, "%FT%H-%M-%S", std::localtime(&triggerTime));

    int index = 1;
    for (auto &image : images)
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

void sendMail(const AlertLevel level, EMailNotifier &notifier, char isoTime[ISO_TIME_BUFFER_SIZE],
        const std::time_t &triggerTime, const float saturation, float fps, const Persistence &storage,
        std::list<std::string> attachmentFiles)
{
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
    strftime(isoTime, ISO_TIME_BUFFER_SIZE, "%FT%H:%M:%S", std::localtime(&triggerTime));
    compose.str("");
    compose << "Possible Intrusion detected (" << isoTime << ") with an image saturation of " << saturation * 100.0
            << "%. " << "Please see attachment for detected image area." << std::endl << "Current FPS are " << fps
            << ".";
    std::string body = compose.str();
    std::cout << body << std::endl;

    // Send EMail
    notifier.alert(subject, body, storage.getLocation(), attachmentFiles);
}
