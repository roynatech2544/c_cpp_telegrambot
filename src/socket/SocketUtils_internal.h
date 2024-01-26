#include <chrono>
#include <string>

#include "TgBotSocket.h"

static constexpr inline auto sleep_sec = std::chrono::seconds(4);

[[nodiscard]] static inline bool handleIncomingBuf(const size_t len, struct TgBotConnection& conn,
                                                   const listener_callback_t& cb, char* errbuf) {
    if (len > 0) {        
        LOG_D("Received buf with %s, invoke callback!", TgBotCmd_toStr(conn.cmd).c_str());

        if (conn.cmd == CMD_EXIT) {
            static std::string exitToken;
            static bool tokenSet = false;
            std::string exitToken_In = conn.data.data_2.token;

            switch (conn.data.data_2.op) {
                case ExitOp::SET_TOKEN:
                    if (!tokenSet) {
                        exitToken = exitToken_In;
                        tokenSet = true;
                        return false;
                    }
                    LOG_W("Token was already set, but SET_TOKEN request, abort.");
                    break;
                case ExitOp::DO_EXIT:
                    if (exitToken != exitToken_In) {
                        LOG_W("Different exit tokens: My: '%s', input: '%s'. Ignoring!", exitToken.c_str(), exitToken_In.c_str());
                        return false;
                    }
                    break;
            }
            return true;
        }
        cb(conn);
    } else {
        LOG_E("Failed to read from socket: %s", errbuf);
    }
    return false;
}
