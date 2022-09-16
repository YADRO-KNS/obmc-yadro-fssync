/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */
#pragma once

#include <fmt/printf.h>

#include <sdeventplus/clock.hpp>
#include <sdeventplus/source/child.hpp>
#include <sdeventplus/source/time.hpp>

#include <filesystem>

namespace fs = std::filesystem;

namespace fssync
{

/**
 * @brief Contains filesystem sync functions.
 */
class Sync
{
  public:
    static constexpr auto clockId = sdeventplus::ClockId::Monotonic;
    using Clock = sdeventplus::Clock<clockId>;
    using Time = sdeventplus::source::Time<clockId>;

    Sync() = delete;
    Sync(const Sync&) = delete;
    Sync& operator=(const Sync&) = delete;
    Sync(Sync&&) = delete;
    Sync& operator=(Sync&&) = delete;
    ~Sync() = default;

    Sync(sdeventplus::Event& event, const fs::path& src, const fs::path& dst);
    void whitelist(const fs::path& filename);
    int processEntry(int mask, const fs::path& entryPath);

  protected:
    void doSync();

    void handleChild(sdeventplus::source::Child& source, const siginfo_t* si);
    void handleTimer(Time& source, Time::TimePoint timePoint);

    template <class R, class P>
    void startTimer(const std::chrono::duration<R, P>& delay)
    {
        timer.set_time(Clock(event).now() + delay);
        timer.set_enabled(sdeventplus::source::Enabled::OneShot);
    }

  private:
    sdeventplus::Event& event;
    fs::path source;
    fs::path destination;
    fs::path whiteListFile;
    std::unique_ptr<sdeventplus::source::Child> childPtr;
    Time timer;
};
} // namespace fssync
