#include "pch.h"

#include "LogParser.h"

using namespace Core::Framework;
#include <fstream>

class LogParserTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a temporary log file in the format Logger writes
        testLogFile = "test_log.txt";
        std::ofstream logFile(testLogFile);
        logFile << "2025-02-09 10:00:00 [INFO] Log message 1\n";
        logFile << "2025-02-09 10:05:00 [ERROR] Log message 2\n";
        logFile << "2025-02-09 10:10:00 [WARNING] Log message 3\n";
        logFile << "2025-02-09 10:15:00 [DEBUG] Log message 4\n";
        logFile << "2025-02-09 10:20:00 [TRACE] Log message 5\n";
        logFile.close();
    }

    void TearDown() override
    {
        // Remove temporary log file after tests
        remove(testLogFile.c_str());
    }

    static void writeLog(const std::string& path, const std::string& content)
    {
        std::ofstream file(path, std::ios::trunc);
        file << content;
    }

    std::string testLogFile;
};

TEST_F(LogParserTest, ParseLogsSuccessfully)
{
    LogParser parser(testLogFile);
    parser.parseLogs();

    auto logs = parser.getAllLogs();
    ASSERT_EQ(logs.size(), 5u);
    EXPECT_EQ(logs[0].m_sSeverity, "INFO");
    EXPECT_EQ(logs[1].m_sSeverity, "ERROR");
    EXPECT_EQ(logs[2].m_sSeverity, "WARNING");
    EXPECT_EQ(logs[3].m_sSeverity, "DEBUG");
    EXPECT_EQ(logs[4].m_sSeverity, "TRACE");
}

TEST_F(LogParserTest, FilterBySeverity)
{
    LogParser parser(testLogFile);
    parser.parseLogs();

    auto errorLogs = parser.filterBySeverity("ERROR");
    ASSERT_EQ(errorLogs.size(), 1u);
    EXPECT_EQ(errorLogs[0].m_sMessage, "Log message 2");

    auto warningLogs = parser.filterBySeverity("WARNING");
    ASSERT_EQ(warningLogs.size(), 1u);
    EXPECT_EQ(warningLogs[0].m_sMessage, "Log message 3");

    auto debugLogs = parser.filterBySeverity("DEBUG");
    ASSERT_EQ(debugLogs.size(), 1u);
    EXPECT_EQ(debugLogs[0].m_sMessage, "Log message 4");

    auto traceLogs = parser.filterBySeverity("TRACE");
    ASSERT_EQ(traceLogs.size(), 1u);
    EXPECT_EQ(traceLogs[0].m_sMessage, "Log message 5");
}

TEST_F(LogParserTest, EmptyLogFile)
{
    std::string emptyFile = "empty_log.txt";
    std::ofstream logFile(emptyFile);
    logFile.close();

    LogParser parser(emptyFile);
    parser.parseLogs();

    auto logs = parser.getAllLogs();
    EXPECT_TRUE(logs.empty());

    remove(emptyFile.c_str());
}

TEST_F(LogParserTest, ParsingTwiceDoesNotDuplicateEntries)
{
    LogParser parser(testLogFile);
    parser.parseLogs();
    parser.parseLogs();

    EXPECT_EQ(parser.getAllLogs().size(), 5u);
}

TEST_F(LogParserTest, TimestampWithMillisecondsIsAccepted)
{
    const std::string path = "millis_log.txt";
    writeLog(path, "2025-02-09 10:00:00.123 [INFO] with millis\n");

    LogParser parser(path);
    parser.parseLogs();

    auto logs = parser.getAllLogs();
    ASSERT_EQ(logs.size(), 1u);
    EXPECT_EQ(logs[0].m_sTimestamp, "2025-02-09 10:00:00.123");
    EXPECT_EQ(logs[0].m_sMessage, "with millis");

    remove(path.c_str());
}

TEST_F(LogParserTest, ContinuationLinesAreAppendedToPreviousEntry)
{
    const std::string path = "continuation_log.txt";
    writeLog(path,
        "2025-02-09 10:00:00 [ERROR] Something failed\n"
        "    at main()\n"
        "    at start()\n"
        "2025-02-09 10:00:01 [INFO] Next\n");

    LogParser parser(path);
    parser.parseLogs();

    auto logs = parser.getAllLogs();
    ASSERT_EQ(logs.size(), 2u);
    EXPECT_EQ(logs[0].m_sMessage, "Something failed\n    at main()\n    at start()");
    EXPECT_EQ(logs[1].m_sMessage, "Next");
    EXPECT_EQ(parser.getSkippedLineCount(), 0u);

    remove(path.c_str());
}

TEST_F(LogParserTest, LinesBeforeFirstEntryAreCountedAsSkipped)
{
    const std::string path = "skipped_log.txt";
    writeLog(path,
        "not a log line\n"
        "\n"
        "2025-02-09 10:00:00 [INFO] first\n");

    LogParser parser(path);
    parser.parseLogs();

    EXPECT_EQ(parser.getAllLogs().size(), 1u);
    EXPECT_EQ(parser.getSkippedLineCount(), 1u);   // the blank line is ignored, not counted

    remove(path.c_str());
}

TEST_F(LogParserTest, MissingFileThrows)
{
    LogParser parser("does_not_exist_log.txt");
    EXPECT_THROW(parser.parseLogs(), std::runtime_error);
}
