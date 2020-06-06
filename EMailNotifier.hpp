#ifndef _EMAILNOTIFIER_HPP_
#define _EMAILNOTIFIER_HPP_

#include <atomic>
#include <list>
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
    virtual ~EMailNotifier() = default;
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
    std::unique_ptr<std::thread> mAlertThread;

    void run(const std::string &command);
    bool isAlertSuccess();
};

#endif //_EMAILNOTIFIER_HPP_
