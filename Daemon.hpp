/*
 * Daemon.hpp
 *
 *  Created on: 06.06.2020
 *      Author: jierr
 */

#ifndef _DAEMON_HPP_
#define _DAEMON_HPP_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

class Daemon
{
public:
    Daemon(const Daemon &other) = delete;
    Daemon(const Daemon &&other) = delete;
    Daemon& operator=(const Daemon &other) = delete;

    virtual ~Daemon() = default;
    virtual bool daemonize();
    virtual int run(int argc, char **argv);
    static std::shared_ptr<Daemon> getInstance();
    void setWorkingDirectory(const std::string &path);

protected:
    Daemon();
    static void signalHandler(int number);
    static std::mutex mLock;
    static std::shared_ptr<Daemon> mDaemon;
    static std::atomic<bool> mRunning;

    const std::string mName;
    const std::string mPidFile;
    pid_t mPid { 0 };
    std::string mWorkingDirectory { "/" };

    void openLog();
};

#endif /* _DAEMON_HPP_ */
