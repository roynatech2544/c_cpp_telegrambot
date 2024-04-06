#include <ConfigManager.h>
#include <EnumArrayHelpers.h>
#include <StringToolsExt.h>
#include <absl/log/log.h>

#include <DurationPoint.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <libos/libfs.hpp>
#include <mutex>
#include <thread>

#include "CompilerInTelegram.h"
#include "popen_wdt/popen_wdt.h"

using std::chrono_literals::operator""ms;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

void CompilerInTg::appendExtArgs(std::stringstream &cmd,
                                 std::string extraargs_in,
                                 std::stringstream &result_out) {
    if (!extraargs_in.empty()) {
        TrimStr(extraargs_in);
        cmd << SPACE << extraargs_in;
        std::stringstream result;
        result_out << "cmd: ";
        result_out << std::quoted(cmd.str());
        result_out << std::endl;
    }
}

void CompilerInTg::runCommand(const Message::Ptr &message, std::string cmd,
                              std::stringstream &res, bool use_wdt) {
    bool hasmore = false, watchdog_bitten = false;
    int count = 0, unique_id = 0;
    std::array<char, BASH_READ_BUF> buf = {};
    size_t buf_len = 0;
    std::error_code ec;

#ifdef LOCALE_EN_US
    static std::once_flag once;
    std::call_once(once, [] { setlocale_enus_once(); });
#endif

    boost::replace_all(cmd, std::string(1, '"'), "\\\"");

    LOG(INFO) << __func__ << ": +++";
    onFailed(message, ErrorType::START_COMPILER);
    LOG(INFO) << "Command: '" << cmd << "'";
    auto dp = DurationPoint();
    auto fp = popen_watchdog(cmd.c_str(), use_wdt ? &watchdog_bitten : nullptr);

    if (!fp) {
        onFailed(message, ErrorType::POPEN_WDT_FAILED);
        return;
    }
    while (fgets(buf.data(), buf.size(), fp)) {
        if (buf_len < BASH_MAX_BUF) {
            res << buf.data();
            buf_len += strlen(buf.data());
        } else {
            hasmore = true;
        }
        count++;
        std::this_thread::sleep_for(50ms);
    }
    if (count == 0) res << EMPTY << std::endl;
    res << std::endl;
    if (hasmore) res << "-> Truncated due to too much output\n";

    if (watchdog_bitten) {
        res << WDT_BITE_STR;
    } else {
        float millis = static_cast<float>(dp.get().count());
        res << "-> It took " << std::fixed << std::setprecision(3)
            << millis * 0.001 << " seconds" << std::endl;
    }
    fclose(fp);
    LOG(INFO) << __func__ << ": ---";
}

static std::optional<std::string> findCommandExe(std::string command) {
    static std::string path;
    static std::once_flag once;
    static bool valid;

    std::call_once(once, [] {
        auto it = ConfigManager::getVariable("PATH");
        valid = it.has_value();
        if (valid) {
            path = it.value();
        }
    });
    if (valid) {
        auto paths = StringTools::split(path, FS::path_env_delimiter);
        std::filesystem::path exePath(command);
        FS::appendExeExtension(exePath);
        for (const auto &path : paths) {
            if (!isEmptyOrBlank(path)) {
                std::filesystem::path p(path);
                p /= exePath;
                if (FS::canExecute(p)) {
                    return {p.string()};
                }
            }
        }
    }
    return {};
}

array_helpers::ArrayElem<ProgrammingLangs, std::vector<std::string>> COMPILER(
    ProgrammingLangs &&lang, std::initializer_list<std::string> &&v) {
    return array_helpers::make_elem<ProgrammingLangs, std::vector<std::string>>(
        std::move(lang), std::move(v));
}

bool findCompiler(ProgrammingLangs lang, std::string &path) {
    static const auto compilers =
        array_helpers::make<static_cast<int>(ProgrammingLangs::MAX),
                            ProgrammingLangs, const std::vector<std::string>>(
            COMPILER(ProgrammingLangs::C, {"clang", "gcc", "cc"}),
            COMPILER(ProgrammingLangs::CXX, {"clang++", "g++", "c++"}),
            COMPILER(ProgrammingLangs::GO, {"go"}),
            COMPILER(ProgrammingLangs::PYTHON, {"python", "python3"}));
    for (const auto &options : array_helpers::find(compilers, lang)->second) {
        auto ret = findCommandExe(options);
        if (ret) {
            path = ret.value();
            return true;
        }
    }
    return false;
}
