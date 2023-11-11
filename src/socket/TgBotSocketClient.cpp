#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>

#include "../include/RuntimeException.h"
#include "TgBotSocket.h"

static void usage(char* argv, bool success) {
    printf("Usage: %s [cmd enum value] [args...]\n\n", argv);
    printf("Available cmd enum values:\n");
    using sortedIt = std::pair<TgBotCommand, std::string>;
    auto sortedVec = std::vector<sortedIt>(kTgBotCommandStrMap.begin(), kTgBotCommandStrMap.end());
    std::sort(sortedVec.begin(), sortedVec.end(), [](sortedIt v1, sortedIt v2) { 
        return v1.second > v2.second; 
    });
    for (const auto& ent : sortedVec) {
        if (ent.first == CMD_EXIT || ent.first == CMD_MAX) continue;
        printf("%s: %d\n", ent.second.c_str(), ent.first);
    }
    exit(success);
    __builtin_unreachable();
}

static std::map<TgBotCommand, int> kRequiredArgsCount = {
    {CMD_WRITE_MSG_TO_CHAT_ID, 2},  // chatid, msg
    {CMD_CTRL_SPAMBLOCK, 1},        // Policy
};

static bool verifyArgsCount(TgBotCommand cmd, int argc) {
    auto it = kRequiredArgsCount.find(cmd);
    if (it == kRequiredArgsCount.end())
        throw runtime_errorf("Cannot find cmd %s in argcount map!", toStr(cmd).c_str());
    bool ret = it->second == argc;
    if (!ret)
        fprintf(stderr, "Invalid argument count %d for cmd %s, %d required\n", argc,
                toStr(cmd).c_str(), it->second);
    return ret;
}

static bool stoi_or(std::string str, int32_t* intval) {
    try {
        *intval = std::stoi(str.c_str());
    } catch (...) {
        fprintf(stderr, "Failed to parse '%s' to int\n", str.c_str());
        return false;
    }
    return true;
}

static bool stol_or(std::string str, int64_t* intval) {
    try {
        *intval = std::stol(str.c_str());
    } catch (...) {
        fprintf(stderr, "Failed to parse '%s' to long\n", str.c_str());
        return false;
    }
    return true;
}

template <class C>
bool verifyWithinEnum(C max, int val) { return val >= 0 && val < max; }

template <class C>
bool parseOneEnum(C* res, C max, const char* str, const char* name) {
    int parsed = 0;
    if (stoi_or(str, &parsed)) {
        if (verifyWithinEnum(max, parsed)) {
            *res = static_cast<C>(parsed);
            return true;
        } else {
            fprintf(stderr, "Cannot convert %s to %s enum value\n", str, name);
        }
    }
    return false;
}

int main(int argc, char** argv) {
    enum TgBotCommand cmd;
    union TgBotCommandUnion data_g;
    char* exe = argv[0];

    if (argc == 1)
        usage(exe, true);

    // Remove exe (argv[0])
    ++argv;
    --argc;

    if (!parseOneEnum(&cmd, CMD_MAX, *argv, "cmd")) goto error;
    if (cmd == CMD_EXIT) {
        fprintf(stderr, "CMD_EXIT is not supported\n");
        goto error;
    }

    // Remove cmd (argv[1])
    ++argv;
    --argc;

    if (!verifyArgsCount(cmd, argc))
        goto error;
    switch (cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID: {
            TgBotCommandData::WriteMsgToChatId data;
            if (!stol_or(argv[0], &data.to)) {
                goto error;
            }
            memset(data.msg, 0, sizeof(data.msg));
            strncpy(data.msg, argv[1], sizeof(data.msg));
            data_g.data_1 = data;
            break;
        }
        case CMD_CTRL_SPAMBLOCK: {
            if (!parseOneEnum(&data_g.data_3, TgBotCommandData::CTRL_MAX, argv[0], "spamblock")) {
                goto error;
            }
            break;
        }
        case CMD_EXIT:
        case CMD_MAX:
            goto error;
        default:
            throw runtime_errorf("Unhandled command value: %d!", cmd);
    };
    writeToSocket({cmd, data_g});
    return EXIT_SUCCESS;
error:
    usage(exe, false);
    return EXIT_FAILURE;
}