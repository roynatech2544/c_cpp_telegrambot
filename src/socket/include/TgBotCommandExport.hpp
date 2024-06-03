#pragma once

// A header export for the TgBot's socket connection

#include <openssl/sha.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "../../include/Types.h"

namespace TgBotSocket {

constexpr int MAX_PATH_SIZE = 256;
constexpr int MAX_MSG_SIZE = 256;

enum class Command : std::int32_t {
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_CTRL_SPAMBLOCK,
    CMD_OBSERVE_CHAT_ID,
    CMD_SEND_FILE_TO_CHAT_ID,
    CMD_OBSERVE_ALL_CHATS,
    CMD_DELETE_CONTROLLER_BY_ID,
    CMD_GET_UPTIME,
    CMD_UPLOAD_FILE_DRY,
    CMD_UPLOAD_FILE,
    CMD_DOWNLOAD_FILE,
    CMD_CLIENT_MAX,

    // Below are internal commands
    CMD_SERVER_INTERNAL_START = 100,
    CMD_GET_UPTIME_CALLBACK = CMD_SERVER_INTERNAL_START,
    CMD_GENERIC_ACK,
    CMD_UPLOAD_FILE_DRY_CALLBACK,
    CMD_DOWNLOAD_FILE_CALLBACK,
    CMD_MAX,
};

/**
 * @brief Header for TgBotCommand Packets
 *
 * Header contains the magic value, command, and the size of the data
 */
struct PacketHeader {
    using length_type = uint64_t;
    constexpr static int64_t MAGIC_VALUE_BASE = 0xDEADFACE;
    // Version number, to be increased on breaking changes
    // 1: Initial version
    // 2: Added crc32 checks to packet data
    // 3: Uploadfile has a sha256sum check, std::array conversions
    constexpr static int DATA_VERSION = 3;
    constexpr static int64_t MAGIC_VALUE = MAGIC_VALUE_BASE + DATA_VERSION;

    int64_t magic = MAGIC_VALUE;  ///< Magic value to verify the packet
    Command cmd{};                ///< Command to be executed
    length_type data_size{};      ///< Size of the data in the packet
    uint32_t checksum{};          ///< Checksum of the packet data
};

namespace data {

enum class FileType {
    TYPE_PHOTO,
    TYPE_VIDEO,
    TYPE_GIF,
    TYPE_DOCUMENT,
    TYPE_DICE,
    TYPE_MAX
};

enum class CtrlSpamBlock {
    CTRL_OFF,              // Disabled
    CTRL_LOGGING_ONLY_ON,  // Logging only, not taking action
    CTRL_ON,               // Enabled, does delete but doesn't mute
    CTRL_ENFORCE,          // Enabled, deletes and mutes
    CTRL_MAX,
};

struct WriteMsgToChatId {
    ChatId chat;                             // destination chatid
    std::array<char, MAX_MSG_SIZE> message;  // Msg to send
};

struct ObserveChatId {
    ChatId chat;
    bool observe;  // new state for given ChatId,
                   // true/false - Start/Stop observing
};

struct SendFileToChatId {
    ChatId chat;                               // Destination ChatId
    FileType fileType;                         // File type for file
    std::array<char, MAX_PATH_SIZE> filePath;  // Path to file (local)
};

struct ObserveAllChats {
    bool observe;  // new state for all chats,
                   // true/false - Start/Stop observing
};

struct DeleteControllerById {
    int controller_id;
};

struct UploadFile {
    std::array<char, MAX_PATH_SIZE> filepath{};  // Destination file name
    std::array<unsigned char, SHA256_DIGEST_LENGTH>
        sha256_hash{};  // SHA256 hash of the file

    // Returns AckType::ERROR_COMMAND_IGNORED on options failure
    struct Options {
        bool overwrite = false;  // Overwrite existing file if exists
        bool hash_ignore =
            false;  // Ignore hash, always upload file even if the same file
                    // exists and the hash matches. Depends on overwrite flag
                    // for actual overwriting
        bool dry_run =
            false;  // If true, just check the hashes and return result.
    } options;
    uint8_t buf[];  // Buffer
};

struct DownloadFile {
    std::array<char, MAX_PATH_SIZE> filepath{};      // Path to file (in remote)
    std::array<char, MAX_PATH_SIZE> destfilename{};  // Destination file name
    uint8_t buf[];                                   // Buffer
};
}  // namespace data

namespace callback {

struct GetUptimeCallback {
    std::array<char, sizeof("Uptime: 999h 99m 99s")> uptime;
};

enum class AckType {
    SUCCESS,
    ERROR_TGAPI_EXCEPTION,
    ERROR_INVALID_ARGUMENT,
    ERROR_COMMAND_IGNORED,
    ERROR_RUNTIME_ERROR,
};

struct GenericAck {
    AckType result;  // result type
    // Error message, only valid when result type is not SUCCESS
    std::array<char, MAX_MSG_SIZE> error_msg{};

    // Create a new instance of the Generic Ack, error.
    explicit GenericAck(AckType result, const std::string& errorMsg)
        : result(result) {
        std::strncpy(this->error_msg.data(), errorMsg.c_str(), MAX_MSG_SIZE);
        this->error_msg[MAX_MSG_SIZE - 1] = '\0';
    }
    GenericAck() = default;

    // Create a new instance of the Generic Ack, success.
    static GenericAck ok() { return GenericAck(AckType::SUCCESS, "OK"); }
};

}  // namespace callback
}  // namespace TgBotSocket

// static asserts for compatibility check
// We make some asserts here to ensure that the size of the packet is expected
#define ASSERT_SIZE(DataType, DataSize)           \
    static_assert(sizeof(DataType) == (DataSize), \
                  "Size of " #DataType " has changed")

namespace TgBotSocket::data {
ASSERT_SIZE(WriteMsgToChatId, 264);
ASSERT_SIZE(ObserveChatId, 16);
ASSERT_SIZE(SendFileToChatId, 272);
ASSERT_SIZE(ObserveAllChats, 1);
ASSERT_SIZE(DeleteControllerById, 4);
ASSERT_SIZE(UploadFile, 291);
ASSERT_SIZE(DownloadFile, 512);
ASSERT_SIZE(PacketHeader, 32);
}

namespace TgBotSocket::callback {
ASSERT_SIZE(GetUptimeCallback, 21);
ASSERT_SIZE(GenericAck, 260);
}

#undef ASSERT_SIZE