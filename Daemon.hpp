/*
 * Daemon.hpp
 *
 *  Created on: 06.06.2020
 *      Author: jierr
 */

#ifndef _DAEMON_HPP_
#define _DAEMON_HPP_

#include <csignal>
#include <memory>
#include <string>

class Daemon
{
public:
    Daemon(const Daemon &other) = delete;
    Daemon(const Daemon &&other) = delete;
    Daemon& operator=(const Daemon &other) = delete;

    virtual ~Daemon() = default;
    virtual bool daemonize();
    virtual int run(int argc, char **argv) = 0;
    void setRunDir(const std::string &path);
    void openLog();

protected:
    explicit Daemon(const std::string &name);
    virtual __sighandler_t getSignalHandler() = 0;

    const std::string mName;
    const std::string mPidFile;
    pid_t mPid { 0 };
    std::string mWorkingDirectory { "/tmp/" };
};

#endif /* _DAEMON_HPP_ */
