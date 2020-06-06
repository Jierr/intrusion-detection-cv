#include "ArgumentParser.hpp"

void ArgumentParser::parse(int argc, char **argv)
{
    for (int arg = 1; arg < argc; ++arg)
    {
        std::string name = argv[arg];
        auto argument = mArgument.find(name);
        if ((argument != mArgument.end()) && (arg < argc - 1))
        {
            argument->second = std::string(argv[arg + 1]);
        }
    }
}

void ArgumentParser::registerArgument(const std::string &parameter, const std::string &value)
{
    mArgument[parameter] = value;
}

ArgumentParser::Argument ArgumentParser::operator[](std::string name)
{
    auto argument = mArgument.find(name);
    if (argument != mArgument.end())
    {
        return
        {   true, argument->first, argument->second};
    }

    return
    {   false, "",""};
}

