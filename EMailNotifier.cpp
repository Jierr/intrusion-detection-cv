#include <chrono>
#include <cstdlib>
#include <ctime>
#include <sstream>

#include "EMailNotifier.hpp"

EMailNotifier::EMailNotifier(const std::string& from, const std::string& to, const std::string& password)
{
	mFrom = from;
	mTo = to;
	mPassword = password;
	
}

EMailNotifier::~EMailNotifier()
{

}

void EMailNotifier::alert(std::string subject, std::string body, std::string attachment)
{
	std::stringstream command;
	auto time = std::chrono::system_clock::now();
	std::time_t cTime = std::chrono::system_clock::to_time_t(time);
	command << "echo \"Subject: " << subject
			<< "\\" << std::endl << std::endl 
			<< body
			<< "\" | sendmail -v " << mTo << "&";
			
	system(command.str().c_str());
}

