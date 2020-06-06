/*
 * Daemon.cpp
 *
 *  Created on: 06.06.2020
 *      Author: jierr
 */

#include "Daemon.hpp"

#include <cstdio>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

std::mutex Daemon::mLock;
std::shared_ptr<Daemon> Daemon::mDaemon;
std::atomic<bool> Daemon::mRunning;

namespace
{
constexpr char DAEMON_NAME[] = "tapocam-intrusion-daemon";
}

Daemon::Daemon()
        :
        mName { DAEMON_NAME }, mPidFile { std::string("/var/run/") + DAEMON_NAME + ".pid" }
{
    mRunning = false;
}

// https://gist.github.com/faberyx/b07d146e11efbad1643f3e8ba6f1a475
bool Daemon::daemonize()
{
    int sid = -1;
    struct sigaction newSigAction;
    sigset_t newSigSet;

    /* Return, if already daemonized */
    if (getppid() == 1)
    {
        return true;
    }

    /* Set signal mask - signals we want to block */
    sigemptyset(&newSigSet);
    sigaddset(&newSigSet, SIGCHLD); /* ignore child - i.e. we don't need to wait for it */
    sigaddset(&newSigSet, SIGTSTP); /* ignore Tty stop signals */
    sigaddset(&newSigSet, SIGTTOU); /* ignore Tty background writes */
    sigaddset(&newSigSet, SIGTTIN); /* ignore Tty background reads */
    sigprocmask(SIG_BLOCK, &newSigSet, NULL); /* Block the above specified signals */

    /* Set up a signal handler */
    newSigAction.sa_handler = &Daemon::signalHandler;
    sigemptyset(&newSigAction.sa_mask);
    newSigAction.sa_flags = 0;

    /* Signals to handle */
    sigaction(SIGHUP, &newSigAction, NULL); /* catch hangup signal */
    sigaction(SIGTERM, &newSigAction, NULL); /* catch term signal */
    sigaction(SIGINT, &newSigAction, NULL); /* catch interrupt signal */

    mPid = fork();
    if (mPid < 0)
        exit(EXIT_FAILURE);
    // Terminate Parent
    if (mPid > 0)
        exit(EXIT_SUCCESS);

    // Now Child
    umask(027);  // Permissions 750
    sid = setsid();
    if (sid < 0)
        exit(EXIT_FAILURE);
    /* close all descriptors */
    for (int descriptor = getdtablesize(); descriptor >= 0; --descriptor)
    {
        close(descriptor);
    }

    /* Open STDIN */
    int defaultDescriptor = open("/dev/null", O_RDWR);

    if (defaultDescriptor < 0)
    {
        syslog(LOG_WARNING, "Could not open stdin as /dev/null");
    }
    else
    {
        /* STDOUT */
        if (dup(defaultDescriptor) < 0)
        {
            syslog(LOG_WARNING, "Could not open stdout as /dev/null");
        }

        /* STDERR */
        if (dup(defaultDescriptor) < 0)
        {
            syslog(LOG_WARNING, "Could not open stderr as /dev/null");
        }
    }

    //change running directory
    if (chdir(mWorkingDirectory.c_str()) != 0)
    {
        syslog(LOG_ERR, "Could not change to working directory.");
        return false;
    }

    /* Ensure only one copy */
    int pidFilehandle = open(mPidFile.c_str(), O_RDWR | O_CREAT, 0600);

    if (pidFilehandle == -1)
    {
        /* Couldn't open lock file */
        syslog(LOG_INFO, "Could not open PID lock file %s, exiting", mPidFile.c_str());
        exit(EXIT_FAILURE);
    }

    /* Try to lock file */
    if (lockf(pidFilehandle, F_TLOCK, 0) == -1)
    {
        /* Couldn't get lock on lock file */
        syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", mPidFile.c_str());
        exit(EXIT_FAILURE);
    }

    /* write pid to lockfile */
    std::string pidString = std::to_string(getpid());

    if (write(pidFilehandle, pidString.c_str(), pidString.length()) != static_cast<ssize_t>(pidString.length()))
    {
        syslog(LOG_ERR, "Could not write pid file -> Aborting.");
        return false;
    }
    return true;
}

int Daemon::run(int argc, char **argv)
{
    return 0;
}

std::shared_ptr<Daemon> Daemon::getInstance()
{
    mLock.lock();
    if (!mDaemon)
    {
        mRunning = false;
        mDaemon = std::shared_ptr<Daemon>(new Daemon());
    }
    mLock.unlock();
    return mDaemon;
}

void Daemon::setWorkingDirectory(const std::string &path)
{
    mWorkingDirectory = path;
}

void Daemon::signalHandler(int sig)
{
    std::cout << "Interrupt signal (" << sig << ") received.\n";
    switch (sig)
    {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        mRunning = false;
        break;
    default:
        break;
    }
}

void Daemon::openLog()
{
    setlogmask(LOG_UPTO(LOG_INFO));
    openlog(mName.c_str(), LOG_CONS | LOG_PERROR, LOG_USER);

    syslog(LOG_INFO, "Daemon starting up");
}
