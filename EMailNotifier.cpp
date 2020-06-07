#include <chrono>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>

#include "EMailNotifier.hpp"

#include <unistd.h>

namespace
{
constexpr char sendScript[] = "idcv-sendEmail.sh";
}

EMailNotifier::EMailNotifier(const std::string &from, const std::string &to, const std::string &sendScriptLocation)
        :
        mFrom(from),
        mTo(to),
        mSend(sendScriptLocation + '/' + sendScript),
        mSendStatus(0),
        mDone(false),
        mAllowCancel(false)
{
}

EMailNotifier::~EMailNotifier()
{
    if (mAlertThread)
    {
        mLock.lock();
        mDone = true;
        mAlertThread.reset(nullptr);
        mLock.unlock();
    }
}

void EMailNotifier::alert(const std::string &subject, const std::string &body, const std::string &path,
        const std::string &attachment)
{
    std::stringstream command;

    command << "echo \"Subject: " << subject << "\\" << std::endl << std::endl << body << "\\" << std::endl << "\""
            << " | (cat - && uuencode " << path << "/" << attachment << " " << attachment << ")" << " | ssmtp -v "
            << mTo;

    cancel();
    mDone = false;
    mAllowCancel = false;
    mAlertThread.reset(new std::thread(&EMailNotifier::run, this, command.str()));
    cancel();
    mAllowCancel = false;
    mAlertThread.reset(new std::thread(&EMailNotifier::run, this, command.str()));
}

void EMailNotifier::alert(const std::string &subject, const std::string &body, const std::string &path,
        const std::list<std::string> &attachments)
{
    std::stringstream command;

    command << mSend << " \"" << mFrom << "\" " << "\"" << mTo << "\" " << "\"" << subject << "\" " << "\"" << body
            << "\" ";

    if (!attachments.empty())
    {
        command << "\"";
        for (const std::string &attachment : attachments)
        {
            command << path << '/' << attachment << " ";
        }
        command << "\"";
    }

    std::cout << "Execute: '" << command.str() << "'" << std::endl;

    cancel();
    mDone = false;
    mAllowCancel = false;
    mAlertThread.reset(new std::thread(&EMailNotifier::run, this, command.str()));
    cancel();
    mAllowCancel = false;
    mAlertThread.reset(new std::thread(&EMailNotifier::run, this, command.str()));

}

bool EMailNotifier::isAlertSuccess()
{
    if (mDone)
    {
        return (mSendStatus == 0);
    }
    return true;
}

void EMailNotifier::run(const std::string &command)
{
    mLock.lock();
    mSendStatus = -1;
    mLock.unlock();
    mSendStatus = system(command.c_str());
    mLock.lock();
    mDone = true;
    mLock.unlock();
}

void EMailNotifier::cancel()
{
    mLock.lock();
    if (mAlertThread)
    {
        do
        {
            std::cerr << "EMailNotifier::cancel Wait for Cancel Approvement." << std::endl;
            usleep(10000);

        } while (!mAllowCancel);

        auto handle = mAlertThread->native_handle();
        std::cerr << "EMailNotifier::cancel Thread Handle = " << handle << std::endl;
        if (pthread_cancel(handle) != 0)
        {
            std::cerr << "EMailNotifier::cancel Could not cancel thread." << std::endl;
        }
        else
        {
            std::cerr << "EMailNotifier::cancel Thread canceled." << std::endl;
            std::cerr << "EMailNotifier::cancel Thread Joining..." << std::endl;
            usleep(10000);
            mAlertThread->join();
            std::cerr << "EMailNotifier::cancel Thread joined." << std::endl;
        }
        mAlertThread.reset(nullptr);
    }
    mLock.unlock();
    std::cerr << "EMailNotifier::cancel Thread unlocked." << std::endl;
}

EMailNotifier::EMailSendStatus EMailNotifier::check()
{
    mLock.lock();
    EMailSendStatus status = EMailSendStatus::None;
    if (mDone && mAlertThread)
    {
        if (mAlertThread->joinable())
        {
            std::cerr << "EMailNotifier::Thread finished -> join" << std::endl;
            mAlertThread->join();
            mAlertThread.reset(nullptr);
        }

        if (isAlertSuccess())
        {
            status = EMailSendStatus::Success;
        }
        else
        {
            status = EMailSendStatus::Error;
        }
    }
    else if (!mDone && mAlertThread)
    {
        status = EMailSendStatus::InProgress;
    }
    mLock.unlock();
    return status;
}

