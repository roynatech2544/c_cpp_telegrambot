#include <ConfigManager.h>
#include <FileSystemLib.h>
#include <Logging.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>

struct ConfigBackendBase {
    std::function<void *(void)> load;
    std::function<bool(const void *priv, const std::string &name, std::string &outvalue)> getVariable;
    const char *name;
    void *priv;
};

static void *env_load(void) { return nullptr; }

static bool env_getVariable(const void *, const std::string &name, std::string &outvalue) {
    char *buf = getenv(name.c_str());
    if (buf) {
        outvalue = buf;
        return true;
    }
    return false;
}

struct file_priv {
    std::map<std::string, std::string> kConfigEntries;
};

static void *file_load(void) {
    std::string home, line;
    static file_priv p{};
    size_t count = 0;

    if (!getHomePath(home)) {
        LOG_E("Cannot find HOME");
        return nullptr;
    }
    const auto confPath = (std::filesystem::path(home) / ".tgbot_config").u8string();
    std::ifstream ifs(confPath);
    if (ifs.fail()) {
        LOG_E("Opening %s failed", confPath.c_str());
        return nullptr;
    }
    while (std::getline(ifs, line)) {
        std::string::size_type pos;
        count++;

        if (pos = line.find('='); pos == std::string::npos) {
            LOG_E("Invalid line in config file: %zu", count);
            continue;
        }
        p.kConfigEntries.emplace(line.substr(0, pos), line.substr(pos + 1));
        LOG_D("%s is '%s'", line.substr(0, pos).c_str(), line.substr(pos + 1).c_str());
    }
    LOG_I("Loaded %zu entries from %s", p.kConfigEntries.size(), confPath.c_str());
    return &p;
}

static bool file_getVariable(const void *p, const std::string &name, std::string &outvalue) {
    auto priv = reinterpret_cast<const file_priv *>(p);
    if (priv) {
        auto it = priv->kConfigEntries.find(name);
        if (it != priv->kConfigEntries.end()) {
            outvalue = it->second;
            return true;
        }
    }
    return false;
}

static struct ConfigBackendBase backends[] = {
    {
        .load = env_load,
        .getVariable = env_getVariable,
        .name = "Env",
    },
    {
        .load = file_load,
        .getVariable = file_getVariable,
        .name = "File",
    }};

namespace ConfigManager {

static bool load(void) {
    static std::once_flag once;
    try {
        std::call_once(once, [] {
            for (size_t i = 0; i < sizeof(backends) / sizeof(ConfigBackendBase); ++i) {
                auto ptr = &backends[i];
                if (!ptr->load || !ptr->getVariable) throw std::runtime_error("Not ready");
                ptr->priv = ptr->load();
            }
        });
    } catch (const std::runtime_error &) {
        return false;
    }
    return true;
}

bool getVariable(const std::string &name, std::string &outvalue) {
    ConfigBackendBase *ptr;

    if (load()) {
        for (size_t i = 0; i < sizeof(backends) / sizeof(ConfigBackendBase); ++i) {
            ptr = &backends[i];
            if (ptr->getVariable(ptr->priv, name, outvalue)) {
                LOG_D("Used '%s' backend for fetching var '%s'", ptr->name, name.c_str());
                return true;
            }
        }
    } else {
        LOG_E("Manager not ready: while loading '%s'", name.c_str());
    }
    return false;
}
}  // namespace ConfigManager
