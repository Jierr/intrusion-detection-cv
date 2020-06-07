#ifndef _EMAILNOTIFIER_HPP_
#define _EMAILNOTIFIER_HPP_

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <thread>

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

    EMailNotifier(const std::string &from, const std::string &to, const std::string &sendScriptLocation);
    virtual ~EMailNotifier();
    void alert(const std::string &subject, const std::string &body, const std::string &path,
            const std::string &attachment);
    void alert(const std::string &subject, const std::string &body, const std::string &path,
            const std::list<std::string> &attachments);

    EMailSendStatus check();
private:
    std::string mFrom;
    std::string mTo;
    std::string mSend;
    std::atomic<int> mSendStatus;
    std::atomic<bool> mDone;
    std::atomic<bool> mAllowCancel;
    std::unique_ptr<std::thread> mAlertThread;
    std::mutex mLock;

    void run(const std::string &command);
    void cancel();

    bool isAlertSuccess();
};

#endif //_EMAILNOTIFIER_HPP_
