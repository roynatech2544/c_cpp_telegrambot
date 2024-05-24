#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <LogSinks.hpp>
#include <TryParseStr.hpp>
#include <boost/crc.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <impl/bot/ClientBackend.hpp>
#include <impl/bot/TgBotPacketParser.hpp>
#include <impl/bot/TgBotSocketFileHelper.hpp>
#include <iostream>
#include <optional>
#include "SharedMalloc.hpp"
#include "TgBotCommandExport.hpp"

#include <SingleThreadCtrl.h>

// Come last
#include <socket/TgBotSocket.h>

[[noreturn]] static void usage(const char* argv, bool success) {
    std::cout << "Usage: " << argv << " [cmd enum value] [args...]" << std::endl
              << std::endl;
    std::cout << "Available cmd enum values:" << std::endl;
    std::cout << TgBotCmd::getHelpText();

    exit(!success);
}

static bool verifyArgsCount(TgBotCommand cmd, int argc) {
    int required = TgBotCmd::toCount(cmd);
    if (required != argc) {
        LOG(ERROR) << "Invalid argument count " << argc << " for cmd "
                   << TgBotCmd::toStr(cmd) << ", " << required << " required";
        return false;
    }
    return true;
}

static void _copyToStrBuf(char dst[], size_t dst_size, char* src) {
    memset(dst, 0, dst_size);
    strncpy(dst, src, dst_size - 1);
}

template <unsigned N>
void copyToStrBuf(char (&dst)[N], char* src) {
    _copyToStrBuf(dst, sizeof(dst), src);
}

template <class C>
bool parseOneEnum(C* res, C max, const char* str, const char* name) {
    int parsed = 0;
    if (try_parse(str, &parsed)) {
        if (max >= 0 && parsed < max) {
            *res = static_cast<C>(parsed);
            return true;
        } else {
            LOG(ERROR) << "Cannot convert " << str << " to " << name
                       << " enum value";
        }
    }
    return false;
}

struct ClientParser : TgBotSocketParser {
    explicit ClientParser(SocketInterfaceBase* interface)
        : TgBotSocketParser(interface) {}
    void handle_CommandPacket(SocketConnContext context,
                              TgBotCommandPacket pkt) override {
        using TgBotCommandData::AckType;
        using TgBotCommandData::GenericAck;
        TgBotCommandData::GetUptimeCallback callbackData = {};
        GenericAck result{};
        std::string resultText;

        switch (pkt.header.cmd) {
            case CMD_GET_UPTIME_CALLBACK: {
                pkt.data.assignTo(callbackData);
                LOG(INFO) << "Server replied: " << callbackData;
                break;
            }
            case CMD_DOWNLOAD_FILE_CALLBACK: {
                fileData_tofile(pkt.data.get(), pkt.header.data_size);
                break;
            }
            case CMD_GENERIC_ACK:
                pkt.data.assignTo(result);
                switch (result.result) {
                    case AckType::SUCCESS:
                        resultText = "Success";
                        break;
                    case AckType::ERROR_TGAPI_EXCEPTION:
                        resultText = "Failed: Telegram Api exception";
                        break;
                    case AckType::ERROR_INVALID_ARGUMENT:
                        resultText = "Failed: Invalid argument";
                        break;
                    case AckType::ERROR_COMMAND_IGNORED:
                        resultText = "Failed: Command ignored";
                        break;
                    case AckType::ERROR_RUNTIME_ERROR:
                        resultText = "Failed: Runtime error";
                        break;
                }
                LOG(INFO) << "Response from server: " << resultText;
                if (result.result != AckType::SUCCESS) {
                    LOG(ERROR) << "Reason: " << result.error_msg;
                }
                break;
            default:
                LOG(ERROR) << "Unhandled callback of command: "
                           << pkt.header.cmd;
                break;
        }
        interface->closeSocketHandle(context);
    }
};

int main(int argc, char** argv) {
    enum TgBotCommand cmd = CMD_MAX;
    std::optional<TgBotCommandPacket> pkt;
    const char* exe = argv[0];

    TgBot_AbslLogInit();
    if (argc == 1) {
        usage(exe, true);
    }
    // Remove exe (argv[0])
    ++argv;
    --argc;

    if (!parseOneEnum(&cmd, CMD_MAX, *argv, "cmd")) {
        LOG(ERROR) << "Invalid cmd enum value";
        usage(exe, false);
    }

    if (TgBotCmd::isInternalCommand(cmd)) {
        LOG(ERROR) << "Internal commands not supported";
        return EXIT_FAILURE;
    }

    // Remove cmd (argv[1])
    ++argv;
    --argc;

    if (!verifyArgsCount(cmd, argc)) {
        usage(exe, false);
    }

    switch (cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID: {
            TgBotCommandData::WriteMsgToChatId data{};
            if (!try_parse(argv[0], &data.to)) {
                break;
            }
            copyToStrBuf(data.msg, argv[1]);
            pkt = TgBotCommandPacket(cmd, data);
            break;
        }
        case CMD_CTRL_SPAMBLOCK: {
            TgBotCommandData::CtrlSpamBlock data;
            if (parseOneEnum(&data, TgBotCommandData::CTRL_MAX, argv[0],
                             "spamblock"))

                pkt = TgBotCommandPacket(cmd, data);
            break;
        }
        case CMD_OBSERVE_CHAT_ID: {
            TgBotCommandData::ObserveChatId data{};
            if (try_parse(argv[0], &data.id) &&
                try_parse(argv[1], &data.observe))
                pkt = TgBotCommandPacket(cmd, data);
        } break;
        case CMD_SEND_FILE_TO_CHAT_ID: {
            TgBotCommandData::SendFileToChatId data{};
            if (try_parse(argv[0], &data.id) &&
                parseOneEnum(&data.type, TgBotCommandData::TYPE_MAX, argv[1],
                             "type")) {
                copyToStrBuf(data.filepath, argv[2]);
                pkt = TgBotCommandPacket(cmd, data);
            }
        } break;
        case CMD_OBSERVE_ALL_CHATS: {
            TgBotCommandData::ObserveAllChats data = false;
            if (try_parse(argv[0], &data)) {
                pkt = TgBotCommandPacket(cmd, data);
            }
        } break;
        case CMD_DELETE_CONTROLLER_BY_ID: {
            SingleThreadCtrlManager::ThreadUsage data{};
            if (parseOneEnum(&data, SingleThreadCtrlManager::USAGE_MAX, argv[0],
                             "usage")) {
                pkt = TgBotCommandPacket(cmd, data);
            }
        }
        case CMD_GET_UPTIME: {
            // Data is unused in this case
            pkt = TgBotCommandPacket(cmd, 1);
            break;
        }
        case CMD_UPLOAD_FILE: {
            pkt = fileData_fromFile(CMD_UPLOAD_FILE, argv[0], argv[1]);
            break;
        }
        case CMD_DOWNLOAD_FILE: {
            TgBotCommandData::DownloadFile data{};
            copyToStrBuf(data.filepath, argv[0]);
            copyToStrBuf(data.destfilename, argv[1]);
            pkt = TgBotCommandPacket(cmd, &data, sizeof(data));
            break;
        }
        default:
            LOG(FATAL) << "Unhandled command: " << TgBotCmd::toStr(cmd);
    };

    if (!pkt) {
        LOG(ERROR) << "Failed parsing arguments for "
                   << TgBotCmd::toStr(cmd).c_str();
        return EXIT_FAILURE;
    } else {
        boost::crc_32_type crc;
        crc.process_bytes(pkt->data.get(), pkt->header.data_size);
        pkt->header.checksum = crc.checksum();
    }

    auto* backend = getClientBackend();
    auto handle = backend->createClientSocket();

    if (handle) {
        backend->writeToSocket(handle.value(), pkt->toSocketData());
        LOG(INFO) << "Sent the command: Waiting for callback...";
        // Handle callbacks
        ClientParser parser(backend);
        parser.onNewBuffer(handle.value());
    }

    return static_cast<int>(!pkt.has_value());
}
