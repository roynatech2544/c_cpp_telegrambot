#include <BotAddCommand.h>
#include <Logging.h>
#include <dlfcn.h>

#include <vector>

#include "cmd_dynamic/cmd_dynamic.h"

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

void loadOneCommand(Bot& bot, const std::string& fname) {
    void* handle = dlopen(fname.c_str(), RTLD_NOW);
    struct dynamicCommand* sym = nullptr;
    command_callback_t fn;
    Dl_info info{};
    void* fnptr = nullptr;
    bool isSupported = true;

    if (!handle) {
        LOG_W("Failed to load: %s", dlerror() ?: "unknown");
        return;
    }
    sym = static_cast<struct dynamicCommand*>(dlsym(handle, DYN_COMMAND_SYM_STR));
    if (!sym) {
        LOG_W("Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in %s", fname.c_str());
        dlclose(handle);
        return;
    }
    libs.emplace_back(handle);
    fn = sym->fn;
    if (!sym->isSupported()) {
        fn = commandStub;
        isSupported = false;
    }
    if (sym->enforced)
        bot_AddCommandEnforced(bot, sym->name, fn);
    else
        bot_AddCommandPermissive(bot, sym->name, fn);

    if (dladdr(sym, &info) < 0) {
        LOG_W("dladdr failed for %s: %s", fname.c_str(), dlerror() ?: "unknown");
    } else {
        fnptr = info.dli_saddr;
    }
    LOG_I("Loaded RT command module from %s", fname.c_str());
    LOG_I("Module dump: { enforced: %d, supported: %d, name: %s, fn: %p }", sym->enforced, isSupported, sym->name, fnptr);
}

void loadCommandsFromFile(Bot& bot, const std::string& filename) {
    std::string line;
    std::ifstream ifs(filename);
    if (ifs) {
        while (std::getline(ifs, line)) {
            static const std::string kModulesDir = "src/cmd_dynamic/";
            loadOneCommand(bot, kModulesDir + line);
        }
    }
}
