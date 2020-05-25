#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>

#include "EMailNotifier.hpp"

EMailNotifier::EMailNotifier(const std::string& from, const std::string& to, const std::string& sendScriptLocation)
: mFrom(from), mTo(to), mSend(sendScriptLocation + "/send.sh")
{
}

void EMailNotifier::alert(const std::string& subject, const std::string& body, const std::string& path, const std::string& attachment)
{
	std::stringstream command;
	
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
	
	command << mSend << " \"" 
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

