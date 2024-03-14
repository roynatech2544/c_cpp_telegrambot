#include <BotReplyMessage.h>

#include <boost/algorithm/string/replace.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <popen_wdt/popen_wdt.hpp>
#include <string>

#include "CommandModule.h"
#include "RTCommandLoader.h"
#include "command_modules/compiler/CompilerInTelegram.h"

void RTLoadCommandFn(Bot& bot, const Message::Ptr message) {
    static int count = 0;
    std::string compiler;
    if (!findCompiler(ProgrammingLangs::CXX, compiler)) {
        bot_sendReplyMessage(bot, message, "No CXX compiler found!");
        return;
    }
    auto p = RTCommandLoader::getModulesInstallPath() /
             ("libdlload_" + std::to_string(count));
    appendDylibExtension(p);
    compiler += " -o " + p.string() +
                " libTgBotCommandModules.a lib/libTgBot.a -I{src}/src/include -I{src}/lib/include -I{src}/src"
                " -include {src}/src/command_modules/runtime/cmd_dynamic.h -std=c++20 -shared";
    boost::replace_all(compiler, "{src}", getSrcRoot().string());
    CompilerInTgForCCppImpl impl(bot, compiler, "dltmp.cc");
    std::filesystem::remove(p);
    impl.run(message);
    if (fileExists(p)) {
        static auto loader = RTCommandLoader(bot);
        if (loader.loadOneCommand(p)) {
            bot_sendReplyMessage(bot, message, "Command loaded!");
            count++;
        } else {
            bot_sendReplyMessage(bot, message, "Failed to load command!");
        }
    }
}

struct CommandModule cmd_rtload("rtload", "Runtime load command",
                                CommandModule::Flags::Enforced |
                                    CommandModule::Flags::HideDescription,
                                {}, RTLoadCommandFn);