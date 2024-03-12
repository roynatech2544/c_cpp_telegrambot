#include <CompilerInTelegram.h>
#include <ExtArgs.h>
#include <filesystem>

// Verify, Parse, Write
bool CompilerInTgForGeneric::verifyParseWrite(const Message::Ptr& message,
                                              std::string& extraargs) {
    if (hasExtArgs(message)) {
        parseExtArgs(message, extraargs);
        if (extraargs == "--path") {
            onResultReady(message, "Selected compiler: " + cmdPrefix);
            return false;  // Bail out
        }
    }
    return verifyAndWriteMessage(message, outfile);
}

void CompilerInTgForGenericImpl::onResultReady(const Message::Ptr& message,
                                               const std::string& text) {
    CompilerInTgHelper::onResultReady(_bot, message, text);
}

void CompilerInTgForGenericImpl::onFailed(const Message::Ptr& who,
                                          const ErrorType e) {
    CompilerInTgHelper::onFailed(_bot, who, e);
}

void CompilerInTgForGeneric::run(const Message::Ptr &message) {
    std::string extargs;
    std::stringstream cmd, res;

    if (verifyParseWrite(message, outfile)) {
        cmd << cmdPrefix << SPACE << outfile;
        
        if (hasExtArgs(message)) {
            parseExtArgs(message, extargs);
            appendExtArgs(cmd, extargs, res);
        }

        runCommand(message, cmd.str(), res);
        onResultReady(message, res.str());
        std::filesystem::remove(outfile);
    }
}
