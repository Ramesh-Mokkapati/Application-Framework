#include "pch.h"

#include "Logger.h"
using namespace Core::Framework;

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

class LoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // The temp folder always exists, unlike a hard-coded path such as C:\Temp
        logFilename = (std::filesystem::temp_directory_path() / "CoreTests_LoggerTest.log").string();
        std::remove(logFilename.c_str());
        logger.setLogFile(logFilename);
        logger.setLogLevel(LogLevel::DEBUG);
    }

    void TearDown() override
    {
        // Logger is a singleton: release the file (Windows can't delete it while open)
        // and leave logging off so later tests start from a known state
        logger.closeLogFile();
        logger.setLogLevel(LogLevel::NONE);
        std::remove(logFilename.c_str());
    }

    std::string readLog() const
    {
        std::ifstream file(logFilename);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    Logger& logger = Logger::getInstance();
    std::string logFilename;
};

TEST_F(LoggerTest, SetLogLevelTest)
{
    logger.setLogLevel(LogLevel::WARN);
    logger.log(LogLevel::INFO, "This should not appear in log");
    logger.log(LogLevel::WARN, "SetLogLevelTest");

    const std::string log = readLog();
    EXPECT_NE(log.find("SetLogLevelTest"), std::string::npos);
    EXPECT_EQ(log.find("This should not appear in log"), std::string::npos);
}

TEST_F(LoggerTest, LogToFileTest)
{
    logger.setLogLevel(LogLevel::INFO);
    logger.log(LogLevel::INFO, "LogToFileTest");

    ASSERT_TRUE(std::filesystem::exists(logFilename)) << "Log file does not exist!";
    EXPECT_NE(readLog().find("LogToFileTest"), std::string::npos) << "Log entry not found in log file!";
}

TEST_F(LoggerTest, LogLevelFilteringTest)
{
    logger.setLogLevel(LogLevel::INFO);
    logger.log(LogLevel::DEBUG, "Filtered out");
    logger.log(LogLevel::INFO, "LogLevelFilteringTest");

    const std::string log = readLog();
    EXPECT_NE(log.find("LogLevelFilteringTest"), std::string::npos);
    EXPECT_EQ(log.find("Filtered out"), std::string::npos);
}

TEST_F(LoggerTest, LogFileWritingTest)
{
    logger.log(LogLevel::INFO, "LogFileWritingTest");

    // "YYYY-MM-DD HH:MM:SS [INFO] message"
    const std::string log = readLog();
    EXPECT_NE(log.find(" [INFO] LogFileWritingTest"), std::string::npos);
}

TEST_F(LoggerTest, NoneLevelDisablesLogging)
{
    logger.setLogLevel(LogLevel::NONE);
    logger.log(LogLevel::ERROR, "Should not be written");

    EXPECT_EQ(readLog().find("Should not be written"), std::string::npos);
}

TEST_F(LoggerTest, ParseLevelIsCaseInsensitiveAndTrimmed)
{
    LogLevel level = LogLevel::NONE;

    EXPECT_TRUE(Logger::tryParseLevel("debug", level));
    EXPECT_EQ(level, LogLevel::DEBUG);

    EXPECT_TRUE(Logger::tryParseLevel("  Warn \t", level));
    EXPECT_EQ(level, LogLevel::WARN);

    EXPECT_EQ(logger.stringToLevel("Error"), LogLevel::ERROR);
}

TEST_F(LoggerTest, UnknownLevelIsRejected)
{
    LogLevel level = LogLevel::INFO;

    EXPECT_FALSE(Logger::tryParseLevel("verbose", level));
    EXPECT_EQ(level, LogLevel::INFO) << "level must be left unchanged";

    EXPECT_EQ(logger.stringToLevel("verbose"), LogLevel::NONE);
}

TEST_F(LoggerTest, LoadConfigReadsTrimmedLogLevel)
{
    const std::string configFile = (std::filesystem::temp_directory_path() / "CoreTests_LoggerConfig.ini").string();
    {
        std::ofstream config(configFile);
        config << "; LogLevel comment = NONE\n";
        config << "log_level = INFO\n";
    }

    logger.setLogLevel(LogLevel::NONE);
    logger.loadConfig(configFile);
    std::remove(configFile.c_str());

    logger.log(LogLevel::DEBUG, "Below INFO");
    logger.log(LogLevel::INFO, "LoadConfigReadsTrimmedLogLevel");

    const std::string log = readLog();
    EXPECT_NE(log.find("LoadConfigReadsTrimmedLogLevel"), std::string::npos);
    EXPECT_EQ(log.find("Below INFO"), std::string::npos);
}
