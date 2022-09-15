/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, KNS Group LLC (YADRO).
 */
#include "whitelist.hpp"

#include <fstream>
#include <string>

namespace fssync
{

inline bool isExcess(const char c)
{
    return isspace(c) || c == '/';
}

void WhiteList::load(const fs::path& fileName)
{
    std::string line;
    std::ifstream file(fileName);
    while (std::getline(file, line))
    {
        ssize_t beg = 0, end = line.length();

        for (; beg < end && isExcess(line.at(beg)); ++beg)
            ;
        for (; end > beg && isExcess(line.at(end - 1)); --end)
            ;

        items.emplace(line.substr(beg, end - beg));
    }
}

bool WhiteList::check(const fs::path& entryPath) const
{
    // NOTE: `lower_bound` leads to less comparison calls than `find`.
    auto it = items.lower_bound(entryPath);
    if (it != items.end())
    {
        return entryPath.string().length() >= it->string().length();
    }
    return false;
}

} // namespace fssync
