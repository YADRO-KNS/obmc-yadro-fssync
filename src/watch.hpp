/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */
#pragma once

#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>
#include <sdeventplus/source/io.hpp>

#include <filesystem>
#include <map>

namespace fs = std::filesystem;

namespace inotify
{

/**
 * @brief Adds inotify watch on persistent files to be synced
 */
class Watch
{
  public:
    using Callback = std::function<void(int, const fs::path&)>;

    Watch() = delete;
    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;
    Watch(Watch&&) = delete;
    Watch& operator=(Watch&&) = delete;

    /**
     * @brief dtor - remove inotify watch and close fd's
     */
    ~Watch();

    static Watch create(sdeventplus::Event& event, const fs::path& root,
                        Callback callback);

  protected:
    /**
     * @brief ctor - hook inotify watch with sd-event
     *
     * @param event    - sd-event object
     * @param fd       - inotify fd
     * @param root     - root directory watched to
     * @param callback - The callback function for processing files
     */
    Watch(sdeventplus::Event& event, int fd, const fs::path& root,
          Callback callback);

    /**
     * @brief Get inotify FD
     */
    inline int inotifyFd() const
    {
        return eventReader.get_fd();
    }

    /**
     * @brief Adds an inotify watch to the specified directory and its sub
     * directories.
     */
    void addWatch(const fs::path& dir);

    /**
     * @brief Handle inotify event
     *
     * @param source - sdevent source object
     * @param fd     - inotify fd
     * @param revent - events mask
     */
    void handleEvent(sdeventplus::source::IO& source, int fd, uint32_t revent);

    /**
     * @brief Scans root directory recursively and (re)adds watches.
     */
    void rescanRoot(sdeventplus::source::EventBase& source);

    /**
     * @brief Check whether if directories to watch exist.
     */
    void checkWds(sdeventplus::source::EventBase& source);

  private:
    sdeventplus::source::IO eventReader;
    sdeventplus::source::Defer rescan;
    sdeventplus::source::Post post;
    std::map<int, fs::path> wds;
    fs::path root;
    Callback syncCallback;
};

} // namespace inotify
