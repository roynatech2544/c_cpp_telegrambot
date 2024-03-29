#include "DatabaseLoader.h"

#include <gtest/gtest.h>

#include <cinttypes>
#include <mutex>

#include "Logging.h"

database::DatabaseWrapper& loadDb() {
    static std::once_flag once;
    std::call_once(once, [] {
        // DatabaseWrapper::load throws std::runtime_error if it fails to load
        // the database.
        ASSERT_NO_THROW(database::DBWrapper.load());
        LOG(LogLevel::INFO, "DB loaded, Owner id %" PRId64,
            database::DBWrapper.maybeGetOwnerId());
    });
    return database::DBWrapper;
}