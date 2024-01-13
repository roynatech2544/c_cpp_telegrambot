#pragma once

#include <string>

#ifdef __WIN32
static inline const char path_env_delimiter = ';';
static inline const char dir_delimiter = '\\';
#else
static inline const char path_env_delimiter = ':';
static inline const char dir_delimiter = '/';
#endif

// TODO Find some way to sync with above
static inline const char unix_dir_delimiter = '/';

// Implemented sperately by OS
bool canExecute(const std::string& path);
bool getHomePath(std::string& buf);