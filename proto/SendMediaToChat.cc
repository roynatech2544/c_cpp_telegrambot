#include <Database.h>
#include <Types.h>
#include <socket/TgBotSocket.h>

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ConfigManager.h"
#include "Logging.h"
#include "socket/SocketInterfaceBase.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

using database::DBWrapper;

[[noreturn]] static void usage(const char* argv0, const int exitCode) {
    std::cerr << "Usage: " << argv0 << " <chatId> <name stored in DB>"
              << std::endl;
    exit(exitCode);
}

int main(int argc, char* const* argv) {
    ChatId chatId = 0;
    TgBotCommandData::SendFileToChatId data = {};
    const auto _usage = std::bind(usage, argv[0], std::placeholders::_1);

    if (argc != 3) {
        _usage(EXIT_FAILURE);
    } else if (!strcmp(argv[1], "-h")) {
        _usage(EXIT_SUCCESS);
    }
    DBWrapper.load();
    try {
        chatId = std::stoll(argv[1]);
    } catch (...) {
        LOG(LogLevel::ERROR, "Failed to convert '%s' to ChatId", argv[1]);
        _usage(EXIT_FAILURE);
    }
    const auto mediaEntries = DBWrapper.protodb.mediatonames();
    std::optional<tgbot::proto::MediaToName> it;
    for (int i = 0; i < mediaEntries.size(); ++i) {
        const auto mediaEntriesIt = mediaEntries.Get(i);
        if (const auto mediaEntriesNames = mediaEntriesIt.names();
            mediaEntriesNames.size() > 0) {
            for (int j = 0; j < mediaEntriesNames.size(); ++j) {
                if (!strcasecmp(mediaEntriesNames.Get(j).c_str(), argv[2])) {
                    it = mediaEntriesIt;
                    break;
                }
            }
        }
    }
    if (!it) {
        LOG(LogLevel::ERROR, "Failed to find entry for name '%s'", argv[2]);
        return EXIT_FAILURE;
    } else {
        LOG(LogLevel::INFO, "Found, sending (fileid %s) to chat %" PRId64,
            it->telegrammediaid().c_str(), chatId);
    }
    strncpy(data.filepath, it->telegrammediaid().c_str(),
            sizeof(data.filepath) - 1);
    data.id = chatId;
    data.type = TYPE_DOCUMENT;

    struct TgBotConnection conn {};
    conn.cmd = CMD_SEND_FILE_TO_CHAT_ID;
    conn.data.data_5 = data;
    SocketInterfaceGetter::getForClient()->writeToSocket(conn);
}
