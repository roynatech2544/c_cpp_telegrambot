#include <BotAddCommand.h>
#include <dlfcn.h>
#include <utils/libutils.h>

#include <vector>

#include "cmd_dynamic/cmd_dynamic.h"

struct DynamicLibraryHolder {
    DynamicLibraryHolder(void* handle) : handle_(handle){};
    ~DynamicLibraryHolder() {
        if (handle_) dlclose(handle_);
    }

   private:
    void* handle_;
};

static std::vector<DynamicLibraryHolder> libs;

void loadOneCommand(Bot& bot, const std::string& fname) {
    void* handle = dlopen(fname.c_str(), RTLD_NOW);
    if (!handle) {
        PRETTYF("Warning; Failed to load: %s", dlerror() ?: "unknown");
        return;
    }
    struct dynamicCommand* sym = (struct dynamicCommand*)dlsym(handle, DYN_COMMAND_SYM_STR);
    if (!sym) {
        PRETTYF("Warning: Failed to lookup symbol '" DYN_COMMAND_SYM_STR "' in %s", fname.c_str());
        return;
    }
    libs.emplace_back(handle);
    if (sym->enforced)
        bot_AddCommandEnforced(bot, sym->name, sym->fn);
    else
        bot_AddCommandPermissive(bot, sym->name, sym->fn);
    PRETTYF("Info: Loaded RT command module from %s", fname.c_str());
    PRETTYF("Info: Module dump: { enforced: %d, name: %s, fn: %p }", sym->enforced, sym->name, sym->fn);
}

void loadCommandsFromFile(Bot& bot, const std::string& filename) {
    std::string data, line;
    ReadFileToString(filename, &data);
    std::stringstream ss(data);
    while (std::getline(ss, line)) {
        static std::string kModulesDir = "modules/";
        loadOneCommand(bot, kModulesDir + line);
    }
}