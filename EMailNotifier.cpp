#include <chrono>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <pthread.h>
#include <sstream>

#include "EMailNotifier.hpp"

#include <unistd.h>

namespace
{
constexpr char SEND_SCRIPT[] = "idcv-sendEmail.sh";
constexpr int MAXIMUM_RUNNING_THREADS { 2 };
constexpr unsigned int EXEC_TIMEOUT_SECONDS { 90 };
const std::string TIMEOUT_COMMAND{"timeout --signal=KILL "};

std::string getTimeoutCommand(const unsigned int& seconds)
{
    return TIMEOUT_COMMAND + std::to_string(EXEC_TIMEOUT_SECONDS) + " ";
}
}


EMailNotifier::ThreadContext::ThreadContext(EMailNotifier *emailNotifier, const ThreadId &prio)
        :
        thread(0), ready(false), done(false), sendStatus(-1), priority(prio), notifier(emailNotifier)
{

}

void EMailNotifier::ThreadContext::signalReady()
{
    ready = true;
}

void EMailNotifier::ThreadContext::signalDone()
{
    mutex.lock();
    done = true;
    mutex.unlock();
}

void EMailNotifier::ThreadContext::waitUntilReady() const
{
    while (!ready)
    {
        sleep(1);
        pthread_yield();
    }
}

bool EMailNotifier::ThreadContext::isDone() const
{
    return done;
}

void* arbiter(void *arg)
{
    EMailNotifier::ThreadContext *context = reinterpret_cast<EMailNotifier::ThreadContext*>(arg);
    std::cout << "arbiter -> Thread started." << std::endl;
    if (context == nullptr)
    {
        return nullptr;
    }

    context->waitUntilReady();
    std::cout << "arbiter ->" << context->thread << " Thread ready. Priority: " << context->priority
            << ", Thread handle: " << context->thread << std::endl;


    pthread_testcancel();
    if (!context->command.empty())
    {
        int status = context->notifier->run(
                getTimeoutCommand(EXEC_TIMEOUT_SECONDS) + context->command);
        context->sendStatus = status;
    }

    context->signalDone();
    return nullptr;
}

bool EMailNotifier::startThread(const std::string &command)
{
    pthread_t thread;

    ++mThreadPriority;
    std::cerr << "startThread -> Start Thread with Id " << mThreadPriority << std::endl;
    mContexts.emplace(mThreadPriority, new ThreadContext(this, mThreadPriority));
    mContexts[mThreadPriority]->command = command;
    if (pthread_create(&thread, NULL, &arbiter, mContexts[mThreadPriority].get()) != 0)
    {
        mContexts.erase(mThreadPriority);
        std::cerr << "startThread -> Start Thread with Id FAILED" << mThreadPriority << std::endl;
        return false;
    }
    mContexts[mThreadPriority]->thread = thread;
    mContexts[mThreadPriority]->signalReady();

    return true;
}

void EMailNotifier::cancelThread(pthread_t thread)
{
    if (pthread_cancel(thread) != 0)
    {
        std::cerr << "cancelThread ->" << thread << " pthread_cancel failed." << std::endl;
        return;
    }
    std::cout << "cancelThread ->" << thread << " pthread_cancel success" << std::endl;

    void *result = nullptr;
    if (pthread_join(thread, &result) != 0)
    {
        std::cerr << "cancelThread ->" << thread << " pthread_join failed." << std::endl;
        return;
    }
    std::cout << "cancelThread ->" << thread << " pthread_join success" << std::endl;

    if (result == PTHREAD_CANCELED)
    {
        std::cout << "cancelThread ->" << thread << " Success." << std::endl;
    }
}

void EMailNotifier::stopThreadWithLowestPriority()
{
    if (mContexts.empty())
        return;

    auto context = mContexts.begin();
    pthread_t thread = context->second->thread;
    context->second->mutex.lock();
    if (!context->second->isDone())
    {
        std::cout << "stopThreadWithLowestPriority ->" << context->second->thread
                << " Thread still running, cancel Thread." << std::endl;
        ThreadId key = context->second->priority;
        cancelThread(thread);
        context->second->mutex.unlock();
        mContexts.erase(key);
    }
    else
    {
        std::cout << "stopThreadWithLowestPriority ->" << context->second->thread
                << " Thread already finished, just cleanup." << std::endl;
        context->second->mutex.unlock();
        cleanupThreads();
    }
}

void EMailNotifier::cleanupThreads()
{
    for (const auto &context : mContexts)
    {
        if (context.second->isDone())
        {
            pthread_t thread = context.second->thread;
            ThreadId key = context.second->priority;
            pthread_join(thread, NULL);
            mContexts.erase(key);
        }
    }
}

void EMailNotifier::terminateAllThreads()
{
    while (getThreadCount() > 0)
    {
        stopThreadWithLowestPriority();
    }
}

int EMailNotifier::getThreadCount()
{
    return mContexts.size();
}

EMailNotifier::EMailNotifier(const std::string &from, const std::string &to, const std::string &sendScriptLocation)
        :
        mFrom(from), mTo(to), mSend(sendScriptLocation + '/' + SEND_SCRIPT), mThreadPriority(0)
{
}

EMailNotifier::~EMailNotifier()
{
    terminateAllThreads();
}

void EMailNotifier::send(const std::string &command)
{
    if (getThreadCount() >= MAXIMUM_RUNNING_THREADS)
    {
        std::cerr << "send -> too many threads, cancel one of them." << std::endl;
        stopThreadWithLowestPriority();
    }
    startThread(command);
}

void EMailNotifier::alert(const std::string &subject, const std::string &body, const std::string &path,
        const std::string &attachment)
{
    std::stringstream command;

    command << "echo \"Subject: " << subject << "\\" << std::endl << std::endl << body << "\\" << std::endl << "\""
            << " | (cat - && uuencode " << path << "/" << attachment << " " << attachment << ")" << " | ssmtp -v "
            << mTo;

    send(command.str());
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

    //std::cout << "Execute: '" << command.str() << "'" << std::endl;
    send(command.str());
}

bool EMailNotifier::isAlertSuccess(const ThreadId &threadId)
{
    if (mContexts[threadId]->isDone())
    {
        return (mContexts[threadId]->sendStatus == 0);
    }
    return true;
}

int EMailNotifier::run(const std::string &command)
{
    auto status = system(command.c_str());
    return status;
}

EMailNotifier::EMailSendStatus EMailNotifier::check()
{
    EMailSendStatus status = EMailSendStatus::None;

    return status;
}

