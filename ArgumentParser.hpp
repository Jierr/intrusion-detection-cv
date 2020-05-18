#ifndef _ARGUMENTPARSER_HPP_
#define _ARGUMENTPARSER_HPP_

#include <map>
#include <string>

class ArgumentParser
{
public:
	struct Argument
	{	
		bool exists;
		std::string name;
		std::string value;
	};
	
	ArgumentParser() = default;
	virtual ~ArgumentParser() = default;
	void parse(int argc, char** argv);
	void registerArgument(const std::string& parameter, const std::string& value);
	Argument operator[](std::string name);
private:
	std::map<std::string, std::string> mArgument;
};

#endif //_ARGUMENTPARSER_HPP_
