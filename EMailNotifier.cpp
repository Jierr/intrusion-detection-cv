#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>

#include "EMailNotifier.hpp"

EMailNotifier::EMailNotifier(const std::string& from, const std::string& to)
: mFrom(from), mTo(to)
{
}

void EMailNotifier::alert(const std::string& subject, const std::string& body, const std::string& path, const std::string& attachment)
{
	std::stringstream command;
	auto time = std::chrono::system_clock::now();
	std::time_t cTime = std::chrono::system_clock::to_time_t(time);
	command << "echo \"Subject: " << subject
			<< "\\" << std::endl << std::endl 
			<< body			
			<< "\\" << std::endl << "\""
			<< " | (cat - && uuencode "<< path << "/" << attachment << " " << attachment << ")"
			<< " | ssmtp -v " << mTo << "&";
			
	system(command.str().c_str());
}

void EMailNotifier::alert(const std::string& subject, const std::string& body, const std::string& path, const std::list<std::string>& attachments)
{
	std::stringstream command;
	auto time = std::chrono::system_clock::now();
	std::time_t cTime = std::chrono::system_clock::to_time_t(time);
	
	command << "./send.sh \"" 
			<< mFrom << "\" " 
			<< "\"" << mTo  << "\" "
			<< "\"" << subject  << "\" "
			<< "\"" << body  << "\" ";	
	
	if(!attachments.empty())
	{
		command << "\"";
		for (const std::string& attachment: attachments)
		{
			command << path << '/' << attachment << " ";
		}
		command << "\"";
	}
	command << " &";		
	
	std::cout << "Execute: '" << command.str() << "'" << std::endl;
	system(command.str().c_str());
}

