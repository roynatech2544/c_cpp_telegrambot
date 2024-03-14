#pragma once

#include <map>
#include <string>
#include <string_view>
#include <filesystem>

struct ResourceManager {
    bool preloadOneFile(std::filesystem::path p);
    void preloadResourceDirectory(void);
    const std::string& getResource(std::filesystem::path filename);
    static std::filesystem::path getResourceRootdir(void);
   private:
    std::map<std::filesystem::path, std::string> kResources;
    const static std::string empty;
};

extern ResourceManager gResourceManager;