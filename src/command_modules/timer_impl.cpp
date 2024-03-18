#include <SingleThreadCtrl.h>
#include <TimerImpl.h>

#include "CommandModule.h"

static void TimerStartCommandFn(const Bot &bot, const Message::Ptr message) {
    auto ctrl = gSThreadManager
        .getController<TimerCommandManager>(
            SingleThreadCtrlManager::USAGE_TIMER_THREAD);
    ctrl->reset();
    ctrl->startTimer(bot, message);
}

static void TimerStopCommandFn(const Bot &bot, const Message::Ptr message) {
    gSThreadManager
        .getController<TimerCommandManager>(
            SingleThreadCtrlManager::USAGE_TIMER_THREAD)
        ->stopTimer(bot, message);
}

struct CommandModule cmd_starttimer(
    "starttimer", "Start timer of the bot",
    CommandModule::Flags::Enforced, TimerStartCommandFn);
struct CommandModule cmd_stoptimer(
    "stoptimer", "Stop timer of the bot",
    CommandModule::Flags::Enforced, TimerStopCommandFn);