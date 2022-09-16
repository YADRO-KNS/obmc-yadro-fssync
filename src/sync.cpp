/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */
#include "sync.hpp"

#include <fmt/printf.h>
#include <unistd.h>

#include <sdeventplus/event.hpp>

namespace fssync
{

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
    fmt::print("SYNC: src='{}', dst='{}'\n", source.c_str(),
               destination.c_str());

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
        fmt::print("SYNC: Process already started\n");
        startTimer(std::chrono::seconds{10});
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        fmt::print("CHILD: starting\n");

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

        fmt::print(stderr, "execv failed, {}\n", strerror(errno));
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
        fmt::print(stderr, "fork failed, {}\n", strerror(errno));
    }
}

void Sync::handleChild(sdeventplus::source::Child& source, const siginfo_t* si)
{
    if (!si)
    {
        fmt::print(stderr, "ERROR: No signinfo awailable!\n");
        return;
    }

    switch (si->si_code)
    {
        case CLD_EXITED:
            if (si->si_status == EXIT_SUCCESS)
            {
                fmt::print("SYNC: successful completed.\n");
            }
            else
            {
                fmt::print(stderr, "SYNC: sync process finished with code {}\n",
                           si->si_status);
            }
            break;

        case CLD_STOPPED:
            fmt::print("SYNC: process {} stopped, status={}\n", si->si_pid,
                       si->si_status);
            source.set_enabled(sdeventplus::source::Enabled::OneShot);
            break;

        case CLD_CONTINUED:
            fmt::print("SYNC: process {} continued, status{}\n", si->si_pid,
                       si->si_status);
            source.set_enabled(sdeventplus::source::Enabled::OneShot);
            break;

        case CLD_KILLED:
            fmt::print(stderr, "SYNC: sync process killed by signal {}\n",
                       si->si_status);
            break;

        case CLD_DUMPED:
            fmt::print(stderr,
                       "SYNC: sync process killed by signal {}, "
                       "and dumped core\n",
                       si->si_status);
            break;

        default:
            fmt::print(stderr,
                       "SYNC: unexpected process termination."
                       "signo={}, code={}, status={}\n",
                       si->si_signo, si->si_code, si->si_status);
            break;
    }
}

void Sync::handleTimer(Time&, Time::TimePoint)
{
    fmt::print("SYNC: Timer alarmed\n");
    doSync();
}

} // namespace fssync
