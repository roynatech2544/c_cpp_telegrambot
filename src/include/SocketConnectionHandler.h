#pragma once

#include <socket/TgBotSocket.h>
#include <tgbot/Bot.h>

using TgBot::Bot;

/**
 * @brief This function is called when a new connection is established
 *
 * @param bot The bot instance
 * @param connection The connection object
 */
extern void socketConnectionHandler(const Bot& bot,
                                    struct TgBotConnection connection);
