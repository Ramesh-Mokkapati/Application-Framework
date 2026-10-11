// VDL_INI_ManagerTest

#include "pch.h"
#include <fstream>
#include <string>
#include <vector>

#include "INI_Manager.h"
using namespace Core::Config;

Logger& logger = Logger::getInstance();

namespace
{
    void writeText(const std::string& path, const std::string& content)
    {
        std::ofstream file(path, std::ios::trunc);
        file << content;
    }
}

// Test fixture: INI_Manager is a singleton, so state from one test would leak
// into the next. clear() resets it before every test.
class INI_ManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        INI_Manager::getInstance(logger).clear();
        remove("config.ini");
    }

    void TearDown() override
    {
        remove("config.ini");
    }
};

// Test loading an INI file
TEST_F(INI_ManagerTest, LoadFile)
{
    remove("config.ini");

    INI_Manager& ini = INI_Manager::getInstance(logger);
    ini.set("Database-1", "Host", "192.168.1.10");
    ini.set("Database-1", "Port", "3306");

    ini.set("Database-2", "Host", "192.168.1.20");
    ini.set("Database-2", "Port", "3306");
    ini.save("config.ini");
    ASSERT_TRUE(ini.load("config.ini")) << "Failed to load INI file";
    remove("config.ini");
}

// Test retrieving values
TEST_F(INI_ManagerTest, GetValue)
{
    remove("config.ini");

    INI_Manager& ini = INI_Manager::getInstance(logger);
    ini.set("Database-1", "Host", "192.168.1.10");
    ini.set("Database-1", "Port", "3306");

    ini.set("Database-2", "Host", "192.168.1.20");
    ini.set("Database-2", "Port", "3306");
    ini.save("config.ini");

    ini.load("config.ini");

    EXPECT_EQ(ini.get("Database-1", "Host"), "192.168.1.10");
    EXPECT_EQ(ini.get("Database-2", "Host"), "192.168.1.20");

    // Test missing key (should return default)
    EXPECT_EQ(ini.get("Database-1", "NonExistentKey", "default_value"), "default_value");
    remove("config.ini");
}


// Test modifying and saving values
TEST_F(INI_ManagerTest, SaveFile)
{
    remove("config.ini");

    INI_Manager& ini = INI_Manager::getInstance(logger);

    ini.set("Database-3", "Host", "192.168.1.30");
    ini.set("Database-3", "Port", "3306");

    ASSERT_TRUE(ini.save("config.ini"));

    INI_Manager& ini2 = INI_Manager::getInstance(logger);
    ini2.load("config.ini");

    EXPECT_EQ(ini2.get("Database-3", "Host"), "192.168.1.30");
    EXPECT_EQ(ini2.get("Database-3", "Port"), "3306");
    remove("config.ini");
}

// Test wildcard section retrieval
TEST_F(INI_ManagerTest, WildcardSections)
{
    remove("config.ini");

    INI_Manager& ini = INI_Manager::getInstance(logger);
    ini.set("Database-1", "Host", "192.168.1.10");
    ini.set("Database-1", "Port", "3306");

    ini.set("Database-2", "Host", "192.168.1.20");
    ini.set("Database-2", "Port", "3306");

    ini.save("config.ini");

    ini.load("config.ini");

    std::vector<std::string> dbSections = ini.getSectionsMatching("Database-");
    ASSERT_EQ(dbSections.size(), 2u);
    EXPECT_EQ(dbSections[0], "Database-1");
    EXPECT_EQ(dbSections[1], "Database-2");

    remove("config.ini");
}

// The prefix is literal: '.' must not behave like a regex wildcard
TEST_F(INI_ManagerTest, SectionPrefixMatchIsLiteral)
{
    INI_Manager& ini = INI_Manager::getInstance(logger);
    ini.set("Task.A", "k", "v");
    ini.set("TaskB", "k", "v");
    ini.set("Other", "k", "v");

    std::vector<std::string> dotted = ini.getSectionsMatching("Task.");
    ASSERT_EQ(dotted.size(), 1u);
    EXPECT_EQ(dotted[0], "Task.A");

    // A single trailing '*' is accepted and ignored
    EXPECT_EQ(ini.getSectionsMatching("Task*").size(), 2u);
}

// A section that appears twice in a file is listed once and its keys are merged
TEST_F(INI_ManagerTest, DuplicateSectionInFileIsListedOnce)
{
    writeText("config.ini", "[A]\nx = 1\n[B]\ny = 2\n[A]\nz = 3\n");

    INI_Manager& ini = INI_Manager::getInstance(logger);
    ASSERT_TRUE(ini.load("config.ini"));

    const std::vector<std::string> expected = { "A", "B" };
    EXPECT_EQ(ini.getAllSections(), expected);
    EXPECT_EQ(ini.get("A", "x"), "1");
    EXPECT_EQ(ini.get("A", "z"), "3");
}

TEST_F(INI_ManagerTest, SetOnExistingSectionDoesNotDuplicateIt)
{
    INI_Manager& ini = INI_Manager::getInstance(logger);
    ini.set("S", "a", "1");
    ini.set("S", "b", "2");

    EXPECT_EQ(ini.getAllSections().size(), 1u);
}

TEST_F(INI_ManagerTest, SectionNamesKeysAndValuesAreTrimmed)
{
    writeText("config.ini", "[ A ]\n  key  =  value  \n");

    INI_Manager& ini = INI_Manager::getInstance(logger);
    ASSERT_TRUE(ini.load("config.ini"));

    EXPECT_EQ(ini.get("A", "key"), "value");
}

// Files saved as "UTF-8 with BOM": the first section header must still be recognised
TEST_F(INI_ManagerTest, Utf8ByteOrderMarkIsIgnored)
{
    writeText("config.ini", "\xEF\xBB\xBF[Logging]\nlog_level = DEBUG\n");

    INI_Manager& ini = INI_Manager::getInstance(logger);
    ASSERT_TRUE(ini.load("config.ini"));

    EXPECT_EQ(ini.get("Logging", "log_level"), "DEBUG");
    EXPECT_EQ(ini.getAllSections(), std::vector<std::string>{ "Logging" });
}

TEST_F(INI_ManagerTest, GlobalKeysBeforeFirstSectionAreReadable)
{
    writeText("config.ini", "g = 1\n[S]\nk = 2\n");

    INI_Manager& ini = INI_Manager::getInstance(logger);
    ASSERT_TRUE(ini.load("config.ini"));

    EXPECT_EQ(ini.get("", "g"), "1");
    EXPECT_EQ(ini.get("S", "k"), "2");
}

TEST_F(INI_ManagerTest, SaveAndLoadRoundTripsGlobalsAndSections)
{
    INI_Manager& ini = INI_Manager::getInstance(logger);
    ini.set("", "g", "1");
    ini.set("S", "k", "v");
    ASSERT_TRUE(ini.save("config.ini"));

    ini.clear();
    ASSERT_TRUE(ini.load("config.ini"));

    EXPECT_EQ(ini.get("", "g"), "1");
    EXPECT_EQ(ini.get("S", "k"), "v");
}

// A failed load reports false and keeps whatever was loaded before
TEST_F(INI_ManagerTest, FailedLoadKeepsPreviousData)
{
    INI_Manager& ini = INI_Manager::getInstance(logger);
    ini.set("S", "k", "v");

    testing::internal::CaptureStderr();
    const bool loaded = ini.load("does_not_exist.ini");
    testing::internal::GetCapturedStderr();

    EXPECT_FALSE(loaded);
    EXPECT_EQ(ini.get("S", "k"), "v");
}

TEST_F(INI_ManagerTest, ClearRemovesEverything)
{
    INI_Manager& ini = INI_Manager::getInstance(logger);
    ini.set("", "g", "1");
    ini.set("S", "k", "v");

    ini.clear();

    EXPECT_TRUE(ini.getAllSections().empty());
    EXPECT_EQ(ini.get("S", "k", "gone"), "gone");
    EXPECT_EQ(ini.get("", "g", "gone"), "gone");
}
