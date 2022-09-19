/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */

#include "config.h"

#include "sync.hpp"
#include "watch.hpp"
#include "whitelist.hpp"

#include <fmt/printf.h>
#include <getopt.h>

#include <sdeventplus/event.hpp>
#include <sdeventplus/source/signal.hpp>

#include <chrono>
#include <csignal>

static void signalHandler(sdeventplus::source::Signal& source,
                          const struct signalfd_siginfo*)
{
    fmt::print("\rSignal {} recieved, terminating...\n", source.get_signal());
    source.get_event().exit(EXIT_SUCCESS);
}

static void printUsage(const char* app)
{
    fmt::print(
        "\nUsage: {} [-h] [-d SECONDS] [-w FILE] <source-dir> <dest-dir>\n",
        app);
    fmt::print(R"(Required arguments:
  source-dir            Path to the source directory.
  dest-dir              Path to the destination directory.

Optional arguments:
  -h, --help            show this help message and exit.
  -d, --delay SECONDS   define delay before sync process starting
  -w, --witelist FILE   path to a file with a list of files to track.
                        File should contain paths relative to source-dri.
                        If not specified, all files from the source directory
                        will be transferred to the destination.
)");
}

int main([[maybe_unused]] int argc, [[maybe_unused]]char* argv[])
{
    fmt::print("obmc-yadro-fssync ver {}\n", PROJECT_VERSION);

    fs::path srcDir, dstDir, whiteListFile;
    std::chrono::seconds delay = std::chrono::minutes{2};

    const struct option opts[] = {
        // clang-format off
        { "help",       no_argument,        0, 'h' },
        { "delay",      required_argument,  0, 'd' },
        { "whitelist",  required_argument,  0, 'w' },
        { 0,            0,                  0,  0  },
        // clang-format on
    };

    int optVal;
    while ((optVal = getopt_long(argc, argv, "hd:w:", opts, nullptr)) != -1)
    {
        switch (optVal)
        {
            case 'h':
                printUsage(argv[0]);
                return EXIT_SUCCESS;

            case 'd':
                try
                {
                    delay = std::chrono::seconds{std::stol(optarg, nullptr, 0)};
                }
                catch (const std::invalid_argument&)
                {
                    fmt::print(stderr, "Invalid delay value!\n");
                    printUsage(argv[0]);
                    return EXIT_FAILURE;
                }
                break;

            case 'w':
                whiteListFile = optarg;
                break;

            default:
                fmt::print(stderr, "Invalid option: {}\n", argv[optind - 1]);
                printUsage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (optind == argc - 2)
    {
        srcDir = argv[optind++];
        dstDir = argv[optind];
    }
    else
    {
        fmt::print(stderr, "Required arguments are not specified!\n");
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!fs::is_directory(srcDir))
    {
        fmt::print(stderr, "Invalid source directory specified!\n");
        return EXIT_FAILURE;
    }

    if (!(fs::is_directory(dstDir) || !fs::exists(dstDir)))
    {
        fmt::print(stderr, "Invalid destination directory specified!\n");
        return EXIT_FAILURE;
    }

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

        fssync::WhiteList whitelist;
        whitelist.load(whiteListFile);

        fssync::Sync sync(event, srcDir, dstDir, delay);
        sync.whitelist(whiteListFile);

        auto syncHandler = [&srcDir, &whitelist, &sync](int mask,
                                                        const fs::path& path) {
            auto entry = fs::relative(path, srcDir);
            if (whitelist.check(entry))
            {
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
