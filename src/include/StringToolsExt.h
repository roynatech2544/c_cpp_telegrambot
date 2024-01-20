#pragma once

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <vector>
#include <string>

#include <tgbot/tools/StringTools.h>

static inline bool isEmptyOrBlank(const std::string& str) {
    return str.empty() || std::all_of(str.begin(), str.end(),
                                      [](char c) { return std::isspace(c); });
}

static inline void TrimStr(std::string& str) {
    boost::trim(str);
}

static inline void splitAndClean(const std::string& text, std::vector<std::string>& out) {
    auto v = StringTools::split(text, '\n');
    auto it = std::remove_if(v.begin(), v.end(), [](const std::string& s) { return isEmptyOrBlank(s); });
    v.erase(it, v.end());
    out = v;
}