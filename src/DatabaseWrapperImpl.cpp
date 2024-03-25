#include <Database.h>

void database::DatabaseWrapperImpl::load() {
    DatabaseWrapper::load(FS::getPathForType(FS::PathType::GIT_ROOT) /
                          std::string(kDatabaseFile));
}