#include <BotAddCommand.h>
#include <Logging.h>
#include <dlfcn.h>

#include <filesystem>
#include <vector>

#include "ConfigManager.h"
#include "command_modules/runtime/cmd_dynamic.h"

struct DynamicLibraryHolder {
    DynamicLibraryHolder(void* handle) : handle_(handle){};
    DynamicLibraryHolder(DynamicLibraryHolder&& other) {
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    ~DynamicLibraryHolder() {
        if (handle_) {
            LOG_D("Handle was at %p", handle_);
            dlclose(handle_);
            handle_ = nullptr;
        }
    }

   private:
    void* handle_;
};

static std::vector<DynamicLibraryHolder> libs;

static void commandStub(const Bot& bot, const Message::Ptr& message) {
    bot_sendReplyMessage(bot, message, "Unsupported command");
}

void loadOneCommand(Bot& bot, const std::filesystem::path _fname) {
    struct dynamicCommandModule* sym = nullptr;
    struct CommandModule* mod = nullptr;
    command_callback_t fn;
    Dl_info info{};
    void *handle, *fnptr = nullptr;
    bool isSupported = true;
    std::string fname = _fname.string();
#ifdef __WIN32
    fname += ".dll";
#else
    fname += ".so";
#endif

    handle = dlopen(fname.c_str(), RTLD_NOW);
    if (!handle) {
        LOG_W("Failed to load: %s", dlerror() ?: "unknown");
        return;
    }
    sym = static_cast<struct dynamicCommandModule*>(
        dlsym(handle, DYN_COMMAND_SYM_STR));
    if (!sym) {
        LOG_W("Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in %s",
              fname.c_str());
        dlclose(handle);
        return;
    }
    libs.emplace_back(handle);
    mod = &sym->mod;
    if (!sym->isSupported()) {
        fn = commandStub;
        isSupported = false;
    } else {
        fn = mod->fn;
    }
    if (mod->isEnforced())
        bot_AddCommandEnforced(bot, mod->command, fn);
    else
        bot_AddCommandPermissive(bot, mod->command, fn);

    if (dladdr(sym, &info) < 0) {
        LOG_W("dladdr failed for %s: %s", fname.c_str(),
              dlerror() ?: "unknown");
    } else {
        fnptr = info.dli_saddr;
    }
    LOG_I("Loaded RT command module from %s", fname.c_str());
    LOG_I("Module dump: { enforced: %d, supported: %d, name: %s, fn: %p }",
          mod->isEnforced(), isSupported, mod->command.c_str(), fnptr);
}

void loadCommandsFromFile(Bot& bot, const std::filesystem::path filename) {
    std::string line;
    std::ifstream ifs(filename.string());
    if (ifs) {
        int argc;
        const char** argv;
        copyCommandLine(CommandLineOp::GET, &argc, &argv);
        auto modulesPath = std::filesystem::path(argv[0]).parent_path() /
                           "src/command_modules/runtime/";
        modulesPath.make_preferred();
        while (std::getline(ifs, line)) {
            loadOneCommand(bot, modulesPath / line);
        }
    }
}
