/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */
#include "sync.hpp"

#include <fmt/printf.h>
#include <unistd.h>

#include <phosphor-logging/log.hpp>
#include <sdeventplus/event.hpp>

namespace fssync
{

using namespace phosphor::logging;

inline fs::path addTrailingSlash(const fs::path& path)
{
    return path.filename().empty() ? path : (path / "");
}

Sync::Sync(sdeventplus::Event& event, const fs::path& src, const fs::path& dst,
           const std::chrono::seconds& delay) :
    event(event),
    source(addTrailingSlash(src)), destination(addTrailingSlash(dst)),
    timer(event, {}, std::chrono::microseconds{1},
          std::bind(&Sync::handleTimer, this, std::placeholders::_1,
                    std::placeholders::_2)),
    defaultDelay(delay)
{
    startTimer(defaultDelay);
}

void Sync::whitelist(const fs::path& filename)
{
    whiteListFile = filename;
}

int Sync::processEntry(int, const fs::path&)
{
    startTimer(defaultDelay);
    return 0;
}

void Sync::doSync()
{
    if (childPtr &&
        childPtr->get_enabled() != sdeventplus::source::Enabled::Off)
    {
        log<level::DEBUG>("SYNC: Process already started");
        startTimer(std::chrono::seconds{10});
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        log<level::INFO>("Start sync process");

        std::vector<const char*> cmd = {
            "/usr/bin/rsync",        "--quiet",  "--archive",
            "--prune-empty-dirs",    "--delete", "--recursive",
            "--delete-missing-args",
        };

        if (!whiteListFile.empty())
        {
            cmd.emplace_back("--files-from");
            cmd.emplace_back(whiteListFile.c_str());
        }
        cmd.emplace_back(source.c_str());
        cmd.emplace_back(destination.c_str());
        cmd.emplace_back(nullptr);

        execv(cmd[0], const_cast<char* const*>(cmd.data()));

        log<level::ERR>("execv failed", entry("ERROR=%s", strerror(errno)));
    }
    else if (pid > 0)
    {
        int options = WEXITED | WSTOPPED | WCONTINUED;
        childPtr = std::make_unique<sdeventplus::source::Child>(
            event, pid, options,
            std::bind(&Sync::handleChild, this, std::placeholders::_1,
                      std::placeholders::_2));
    }
    else
    {
        log<level::ERR>("fork failed", entry("ERROR=%s", strerror(errno)));
    }
}

void Sync::handleChild(sdeventplus::source::Child& source, const siginfo_t* si)
{
    if (!si)
    {
        log<level::ERR>("No signinfo available!");
        return;
    }

    switch (si->si_code)
    {
        case CLD_EXITED:
            if (si->si_status == EXIT_SUCCESS)
            {
                log<level::INFO>("Sync process successful completed.");
            }
            else
            {
                log<level::WARNING>("Sync process finished with non zero code",
                                    entry("CODE=%d", si->si_status));
            }
            break;

        case CLD_STOPPED:
            log<level::INFO>("Sync process stopped",
                             entry("STATUS=%d", si->si_status));
            source.set_enabled(sdeventplus::source::Enabled::OneShot);
            break;

        case CLD_CONTINUED:
            log<level::INFO>("Sync process continued",
                             entry("STATUS=%d", si->si_status));
            source.set_enabled(sdeventplus::source::Enabled::OneShot);
            break;

        case CLD_KILLED:
            log<level::WARNING>("Sync process killed by signal",
                                entry("SIGNAL=%d", si->si_status));
            break;

        case CLD_DUMPED:
            log<level::WARNING>("Sync process killed by signal and dumped core",
                                entry("SIGNAL=%d", si->si_status));
            break;

        default:
            log<level::ERR>("Unexpected sync process termination",
                            entry("SIGNO=%d", si->si_signo),
                            entry("CODE=%d", si->si_code),
                            entry("STATUS=%d", si->si_status));
            break;
    }
}

void Sync::handleTimer(Time&, Time::TimePoint)
{
    doSync();
}

} // namespace fssync
