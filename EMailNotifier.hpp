#ifndef _EMAILNOTIFIER_HPP_
#define _EMAILNOTIFIER_HPP_

#include <string>

class EMailNotifier
{
public:
	EMailNotifier(const std::string& from, const std::string& to, const std::string& password);
	virtual ~EMailNotifier();
	void alert(std::string subject, std::string body, std::string attachment);
private:
	std::string mFrom;
	std::string mTo;
	std::string mPassword;
};

#endif //_EMAILNOTIFIER_HPP_
