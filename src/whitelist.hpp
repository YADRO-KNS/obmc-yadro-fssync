/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */
#pragma once

#include <filesystem>
#include <set>

namespace fs = std::filesystem;

namespace fssync
{

namespace details
{
struct PathsComparer
{
    /**
     * @brief Compare the beginning of strings.
     *
     * For example:
     *   Comparison of strings like `etc/systemd` and `etc/systemd/network`
     *   should return true.
     */
    bool operator()(const std::string& lhs, const std::string& rhs) const
    {
        const auto llen = lhs.length();
        const auto rlen = rhs.length();
        return lhs.compare(0, llen, rhs, 0, std::min(llen, rlen)) < 0;
    }
};
} // namespace details

/**
 * @brief Provides functions to filter filesystem entries.
 */
class WhiteList
{
  public:
    WhiteList() = default;
    WhiteList(const WhiteList&) = delete;
    WhiteList& operator=(const WhiteList&) = delete;
    WhiteList(WhiteList&&) = delete;
    WhiteList& operator=(WhiteList&&) = delete;
    ~WhiteList() = default;

    /**
     * @brief Load filter entries from a text file
     */
    void load(const fs::path& filename);

    /**
     * @brief Check whether if filesystem entry is allowed.
     */
    bool check(const fs::path& entryPath) const;

  private:
    std::set<fs::path, details::PathsComparer> items;
};

} // namespace fssync
