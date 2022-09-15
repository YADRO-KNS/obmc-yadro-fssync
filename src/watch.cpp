/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */
#include "watch.hpp"

#include <fmt/format.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <climits>
#include <stdexcept>

namespace inotify
{

Watch::Watch(sdeventplus::Event& event, int fd, const fs::path& root,
             Callback callback) :
    eventReader(event, fd, EPOLLIN,
                std::bind(&Watch::handleEvent, this, std::placeholders::_1,
                          std::placeholders::_2, std::placeholders::_3)),
    rescan(event, std::bind(&Watch::rescanRoot, this, std::placeholders::_1)),
    post(event, std::bind(&Watch::checkWds, this, std::placeholders::_1)),
    root(root), syncCallback(callback)
{}

Watch::~Watch()
{
    auto fd = inotifyFd();
    if (-1 != fd)
    {
        for (const auto& [wd, path] : wds)
        {
            inotify_rm_watch(fd, wd);
        }
        close(fd);
    }
}

Watch Watch::create(sdeventplus::Event& event, const fs::path& root,
                    Watch::Callback callback)
{
    auto fd = inotify_init1(IN_NONBLOCK);
    if (-1 == fd)
    {
        throw std::runtime_error(
            fmt::format("inotify_init1() failed, {}", strerror(errno)));
    }

    return Watch(event, fd, root, callback);
}

static void rmWatch(int fd, int wd, const fs::path& path)
{
    inotify_rm_watch(fd, wd);
    fmt::print("* Remove fd={}, wd={}, '{}': {}\n", fd, wd, path.c_str(),
               strerror(errno));
}

void Watch::handleEvent(sdeventplus::source::IO&, int fd, uint32_t)
{
    constexpr auto bufferSize = sizeof(struct inotify_event) + NAME_MAX + 1;
    std::array<char, bufferSize> buffer;

    auto bytes = read(fd, buffer.data(), buffer.size());
    auto offset = 0;
    while (bytes - offset >= static_cast<long>(sizeof(struct inotify_event)))
    {
        auto evt =
            reinterpret_cast<struct inotify_event*>(buffer.data() + offset);

        fmt::print("INOTIFY: {:08X} ({:08X}), wd={}, name: {}\n", evt->mask,
                   evt->cookie, evt->wd, evt->len > 0 ? evt->name : "(null)");

        offset += sizeof(*evt) + evt->len;

        auto it = wds.find(evt->wd);
        if (it == wds.end())
        {
            continue;
        }

        if (syncCallback)
        {
            syncCallback(evt->mask,
                         evt->len > 0 ? it->second / evt->name : it->second);
        }

        // Add watch for the new directories
        if ((evt->mask & IN_ISDIR) &&
            ((evt->mask & IN_CREATE) || (evt->mask & IN_MOVED_TO)))
        {
            addWatch(it->second / evt->name);
        }

        auto fd = inotifyFd();
        // Remove watch object for deleted directory
        if (evt->mask & IN_DELETE_SELF)
        {
            rmWatch(fd, it->first, it->second);
            wds.erase(it);
        }

        if (evt->mask & IN_IGNORED)
        {
            rmWatch(fd, it->first, it->second);
            auto dir = it->second;
            wds.erase(it);

            // Watch was remove, re-add it if directory still exists.
            if (fs::is_directory(dir))
            {
                addWatch(dir);
            }
        }

        // The directory could be moved to or from outside the root,
        // so we should re-scan all the tree.
        if (evt->mask & IN_MOVE_SELF)
        {
            fmt::print("* Rescan root pending\n");
            rescan.set_enabled(sdeventplus::source::Enabled::OneShot);
        }

        // TODO: Remove this DEBUG trix in production version.
        static const std::string showWds = ".show_wds";
        if ((evt->mask & IN_CLOSE_WRITE) && (it->second == root) &&
            (showWds == evt->name))
        {
            fmt::print("* Show WDS: has {} items:\n", wds.size());
            for (const auto& [wd, path] : wds)
            {
                fmt::print("   wd={}, '{}'\n", wd, path.c_str());
            }
        }
    }
}

void Watch::rescanRoot(sdeventplus::source::EventBase&)
{
    fmt::print("DEFER: Rescan root\n");
    auto fd = inotifyFd();
    for (auto it = wds.begin(); it != wds.end();)
    {
        if (!fs::is_directory(it->second))
        {
            rmWatch(fd, it->first, it->second);
            it = wds.erase(it);
        }
        else
        {
            ++it;
        }
    }
    addWatch(root);
}

void Watch::checkWds(sdeventplus::source::EventBase& source)
{
    if (wds.empty())
    {
        fmt::print(stderr, "ERROR: No directories to watch exist.\n");
        source.get_event().exit(ENOENT);
    }
}

static int createWatch(int fd, const fs::path& path)
{
    constexpr auto flags = IN_CLOSE_WRITE | IN_ATTRIB | IN_CREATE | IN_DELETE |
                           IN_MOVE | IN_MOVE_SELF | IN_DELETE_SELF;
    auto wd = inotify_add_watch(fd, path.c_str(), flags);
    if (-1 == wd)
    {
        throw std::runtime_error(fmt::format("inotify_add_watch({}) failed, {}",
                                             path.c_str(), strerror(errno)));
    }
    fmt::print("* Add wd={}, '{}'\n", wd, path.c_str());
    return wd;
}

void Watch::addWatch(const fs::path& path)
{
    if (!fs::is_directory(path))
    {
        throw std::runtime_error(
            fmt::format("'{}' is not a directory", path.c_str()));
    }

    auto fd = inotifyFd();
    auto wd = createWatch(fd, path);
    wds[wd] = path;
    for (const auto& entry : fs::recursive_directory_iterator(path))
    {
        if (entry.is_directory())
        {
            wd = createWatch(fd, entry);
            wds[wd] = entry;
        }
    }
}

} // namespace inotify
