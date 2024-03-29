#include <ConfigManager.h>
#include <gtest/gtest.h>
#include <cstring>

#include "DatabaseLoader.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 0
#endif

class ConfigManagerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#if _POSIX_C_SOURCE > 200112L || defined __APPLE__
        // Set up the mock environment variables
        setenv("VAR_NAME", "VAR_VALUE", 1);
#endif
#ifdef _WIN32
        _putenv("VAR_NAME=VAR_VALUE");
#endif
        loadDb();
    }

    void TearDown() override {
#if _POSIX_C_SOURCE > 200112L || defined __APPLE__
        unsetenv("VAR_NAME");
#endif
    }
};

TEST_F(ConfigManagerTest, GetVariableEnv) {
    auto it = ConfigManager::getVariable("VAR_NAME");
    EXPECT_TRUE(it.has_value());
    EXPECT_EQ(it.value(), "VAR_VALUE");
}

TEST_F(ConfigManagerTest, CopyCommandLine) {
    int in_argc = 2;
    char buf[16], buf2[16];
    strncpy(buf, "ConfigManagerTest", sizeof(buf));
    strncpy(buf2, "test", sizeof(buf2));
    char *const ins_argv[] = {buf, buf2};
    char *const *ins_argv2 = ins_argv;
    copyCommandLine(CommandLineOp::INSERT, &in_argc, &ins_argv2);

    char *const * out_argv = nullptr;
    int out_argc = 0;
    copyCommandLine(CommandLineOp::GET, &out_argc, &out_argv);
    EXPECT_EQ(out_argc, 2);
    EXPECT_STREQ(out_argv[0], ins_argv[0]);
    EXPECT_STREQ(out_argv[1], ins_argv[1]);
}
