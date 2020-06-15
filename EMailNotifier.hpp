#ifndef _EMAILNOTIFIER_HPP_
#define _EMAILNOTIFIER_HPP_

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <pthread.h>
#include <queue>
#include <stack>

class EMailNotifier
{
public:
    enum EMailSendStatus
    {
        None = 0,
        InProgress,
        Success,
        Error,
    };

    using ThreadId = unsigned int;

    class ThreadContext
    {
    public:
        std::atomic<pthread_t> thread;
        std::atomic<bool> ready;
        std::atomic<bool> done;
        std::atomic<int> sendStatus;
        std::atomic<ThreadId> priority;
        std::string command;
        EMailNotifier *notifier;
        std::mutex mutex;

        ThreadContext();
        ThreadContext(EMailNotifier *emailNotifier, const ThreadId &priority);
        ThreadContext(const ThreadContext &other) = delete;
        ThreadContext(ThreadContext &&other) = delete;
        ThreadContext& operator=(const ThreadContext &other) = delete;
        virtual ~ThreadContext() = default;
        void signalReady();
        void signalDone();
        bool isDone() const;
        void waitUntilReady() const;
    };

    EMailNotifier(const std::string &from, const std::string &to, const std::string &sendScriptLocation);
    virtual ~EMailNotifier();
    void alert(const std::string &subject, const std::string &body, const std::string &path,
            const std::string &attachment);
    void alert(const std::string &subject, const std::string &body, const std::string &path,
            const std::list<std::string> &attachments);

    EMailSendStatus check();
    int run(const std::string &command);

private:
    std::string mFrom;
    std::string mTo;
    std::string mSend;
    ThreadId mThreadPriority;
    std::map<ThreadId, std::shared_ptr<ThreadContext> > mContexts;

    bool isAlertSuccess(const ThreadId &threadPriority);

    bool startThread(const std::string &command);
    void cancelThread(pthread_t thread);
    void stopThreadWithLowestPriority();
    void cleanupThreads();
    void terminateAllThreads();
    int getThreadCount();
    void send(const std::string &command);
};

#endif //_EMAILNOTIFIER_HPP_
