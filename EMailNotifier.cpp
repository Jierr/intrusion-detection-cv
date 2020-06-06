#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <sstream>

#include "EMailNotifier.hpp"

EMailNotifier::EMailNotifier(const std::string &from, const std::string &to, const std::string &sendScriptLocation)
        :
        mFrom(from), mTo(to), mSend(sendScriptLocation + "/send.sh"), mSendStatus(0), mDone(false)
{
}

void EMailNotifier::alert(const std::string &subject, const std::string &body, const std::string &path,
        const std::string &attachment)
{
    std::stringstream command;

    command << "echo \"Subject: " << subject << "\\" << std::endl << std::endl << body << "\\" << std::endl << "\""
            << " | (cat - && uuencode " << path << "/" << attachment << " " << attachment << ")" << " | ssmtp -v "
            << mTo;

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

    mAlertThread.reset(new std::thread(&EMailNotifier::run, this, command.str()));

    return;
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
    mDone = false;
    mSendStatus = -1;
    mSendStatus = system(command.c_str());
    mDone = true;
}

EMailNotifier::EMailSendStatus EMailNotifier::check()
{
    EMailSendStatus status = EMailSendStatus::None;
    if (mDone)
    {
        if (mAlertThread && mAlertThread->joinable())
        {
            mAlertThread->join();
            mAlertThread.reset(nullptr);

            if (isAlertSuccess())
            {
                status = EMailSendStatus::Success;
            }
            else
            {
                status = EMailSendStatus::Error;
            }
        }
    }
    else if (mAlertThread && !mAlertThread->joinable())
    {
        status = EMailSendStatus::InProgress;
    }
    else
    {
        mDone = true;
    }
    return status;
}

