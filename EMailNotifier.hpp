#ifndef _EMAILNOTIFIER_HPP_
#define _EMAILNOTIFIER_HPP_

#include <list>
#include <string>

class EMailNotifier
{
public:
	EMailNotifier(const std::string& from, const std::string& to, const std::string& sendScriptLocation);
	virtual ~EMailNotifier() = default;
	void alert(const std::string& subject, const std::string& body, const std::string& path, const std::string& attachment);
	void alert(const std::string& subject, const std::string& body, const std::string& path, const std::list<std::string>& attachments);
private:
	std::string mFrom;
	std::string mTo;
	std::string mSend;
};

#endif //_EMAILNOTIFIER_HPP_
