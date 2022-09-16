/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */

#include "config.h"

#include "sync.hpp"
#include "watch.hpp"
#include "whitelist.hpp"

#include <fmt/printf.h>

#include <sdeventplus/event.hpp>
#include <sdeventplus/source/signal.hpp>

#include <csignal>

static void signalHandler(sdeventplus::source::Signal& source,
                          const struct signalfd_siginfo*)
{
    fmt::print("\rSignal {} recieved, terminating...\n", source.get_signal());
    source.get_event().exit(EXIT_SUCCESS);
}

int main([[maybe_unused]] int argc, [[maybe_unused]]char* argv[])
{
    fmt::print("obmc-yadro-fssync ver {}\n", PROJECT_VERSION);

    auto event = sdeventplus::Event::get_default();

    try
    {
        sigset_t ss;
        if (sigemptyset(&ss) < 0 || sigaddset(&ss, SIGTERM) < 0 ||
            sigaddset(&ss, SIGINT) < 0 || sigaddset(&ss, SIGCHLD) < 0)
        {
            fmt::print(stderr, "ERROR: Failed to setup signal handlers, {}\n",
                       strerror(errno));
            return EXIT_FAILURE;
        }

        if (sigprocmask(SIG_BLOCK, &ss, nullptr) < 0)
        {
            fmt::print(stderr, "ERROR: Faile to block signals, {}\n",
                       strerror(errno));
            return EXIT_FAILURE;
        }

        sdeventplus::source::Signal sigterm(event, SIGTERM, signalHandler);
        sdeventplus::source::Signal sigint(event, SIGINT, signalHandler);

        fs::path srcDir("test");
        fs::path dstDir("test-dst");
        fs::path whiteListFile("whitelist.txt");

        fssync::WhiteList whitelist;
        whitelist.load(whiteListFile);

        fssync::Sync sync(event, srcDir, dstDir);
        sync.whitelist(whiteListFile);

        auto syncHandler = [&srcDir, &whitelist, &sync](int mask,
                                                        const fs::path& path) {
            auto entry = fs::relative(path, srcDir);
            if (whitelist.check(entry))
            {
                fmt::print("SYNC: mask={:08X}, '{}'\n", mask, entry.c_str());
                sync.processEntry(mask, entry);
            }
        };

        auto watch =
            inotify::Watch::create(event, srcDir, std::move(syncHandler));

        auto rc = event.loop();
        fmt::print("Bye!\n");
        return rc;
    }
    catch (const std::exception& e)
    {
        fmt::print(stderr, "EXCEPTION: {}\n", e.what());
    }

    return EXIT_FAILURE;
}
