#include <Authorization.h>
#include <NamespaceImport.h>
#include <SpamBlock.h>

#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "../utils/libutils.h"

using std::chrono_literals::operator""s;
using TgBot::ChatPermissions;

#ifdef SOCKET_CONNECTION
CtrlSpamBlock gSpamBlockCfg = CTRL_LOGGING_ONLY_ON;
#endif

void spamBlocker(const Bot& bot, const Message::Ptr &message) {
    static std::map<ChatId, ChatHandle> buffer;
    static std::map<ChatId, int> buffer_sub;
    static std::mutex m;  // Protect buffer, buffer_sub
    static auto spamDetectFunc = [&](decltype(buffer)::const_iterator handle) {
        SpamMapT MaxSameMsgMap, MaxMsgMap;
        // By most msgs sent by that user
        for (const auto &perUser : handle->second) {
            std::apply([&](const auto &first, const auto &second) {
                MaxMsgMap.emplace(first, second);
                LOG_D("Chat: %ld, User: %ld, msgcnt: %ld", handle->first,
                      first, second.size());
            }, perUser);
        }

        // By most common msgs
        static auto commonMsgdataFn = [](const Message::Ptr &m) {
            if (m->sticker)
                return m->sticker->fileUniqueId;
            else if (m->animation)
                return m->animation->fileUniqueId;
            else
                return m->text;
        };
        for (const auto &pair : handle->second) {
            std::multimap<std::string, Message::Ptr> byMsgContent;
            for (const auto &obj : pair.second) {
                byMsgContent.emplace(commonMsgdataFn(obj), obj);
            }
            std::unordered_map<std::string, std::vector<Message::Ptr>> commonMap;
            for (const auto &cnt : byMsgContent) {
                commonMap[cnt.first].emplace_back(cnt.second);
            }
            // Find the most common value
            auto mostCommonIt = std::max_element(commonMap.begin(), commonMap.end(),
                                                 [](const auto &lhs, const auto &rhs) {
                                                     return lhs.second.size() < rhs.second.size();
                                                 });
            MaxSameMsgMap.emplace(pair.first, mostCommonIt->second);
            LOG_D("Chat: %ld, User: %ld, maxIt: %ld", handle->first,
                  pair.first, mostCommonIt->second.size());
        }
        static auto deleteAndMute = [&](const Bot& bot_, const SpamMapT &map, const int threshold) {
            // Initial set - all false set
            auto perms = std::make_shared<ChatPermissions>();
            for (const auto &mapmsg : map) {
                if (mapmsg.second.size() >= threshold) {
#ifdef SOCKET_CONNECTION
                    if (gSpamBlockCfg == CTRL_ON) {
                        for (const auto &msg : mapmsg.second) {
                            try {
                                bot_.getApi().deleteMessage(handle->first, msg->messageId);
                            } catch (const std::exception &ignored) {
                            }
                        }
                        try {
                            bot_.getApi().restrictChatMember(handle->first, mapmsg.first,
                                                             perms, 5 * 60);
                        } catch (const std::exception &e) {
                            LOG_W("Cannot mute user %ld in chat %ld: %s", mapmsg.first,
                                  handle->first, e.what());
                        }
#else
                    if (false) {
#endif
                    } else {
                        LOG_I("Spam detected for user %ld", mapmsg.first);
                    }
                }
            }
        };
        deleteAndMute(bot, MaxSameMsgMap, 3);
        deleteAndMute(bot, MaxMsgMap, 5);
    };

#ifdef SOCKET_CONNECTION
    // Global cfg
    if (gSpamBlockCfg == CTRL_OFF) return;
#endif
    // I'm allowed always
    if (Authorized(message, true)) return;
    // Do not track older msgs and consider it as spam.
    if (std::time(0) - message->date > 15) return;
    // We care GIF, sticker, text spams only
    if (!message->animation && message->text.empty() && !message->sticker)
        return;

    static std::once_flag once;
    std::call_once(once, [] {
        std::thread([] {
            while (true) {
                {
                    const std::lock_guard<std::mutex> _(m);
                    auto its = buffer_sub.begin();
                    while (its != buffer_sub.end()) {
                        decltype(buffer)::const_iterator it = buffer.find(its->first);
                        if (it == buffer.end()) {
                            its = buffer_sub.erase(its);
                            continue;
                        }
                        LOG_D("Chat: %ld, Count: %d", its->first, its->second);
                        if (its->second >= 5) {
                            LOG_D("Launching spamdetect for %ld", its->first);
                            spamDetectFunc(it);
                        }
                        buffer.erase(it);
                        its->second = 0;
                        ++its;
                    }
                }
                std::this_thread::sleep_for(5s);
            }
        }).detach();
    });
    {
        const std::lock_guard<std::mutex> _(m);
        buffer[message->chat->id][message->from->id].emplace_back(message);
        ++buffer_sub[message->chat->id];
    }
}