#include "pch.h"

#include "CommandLineParser.h"
using namespace Core::Framework;


// Test Fixture for CommandLineParser
class CommandLineParserTest : public ::testing::Test
{
protected:
    CommandLineParser parser;
};

// Test Constructor (Default)
TEST_F(CommandLineParserTest, DefaultConstructor)
{
    EXPECT_NO_THROW(CommandLineParser parser);
}

// Test Constructor with Arguments
TEST_F(CommandLineParserTest, ConstructorWithArgs)
{
    const char* argv[] = { "program", "--flag", "--option", "value" };
    int argc = sizeof(argv) / sizeof(argv[0]);

    EXPECT_NO_THROW(CommandLineParser parser(argc, const_cast<char**>(argv)));
}

// Test Adding a Flag
TEST_F(CommandLineParserTest, AddFlag)
{
    parser.addFlag("--verbose", "Enable verbose logging");
    EXPECT_FALSE(parser.hasFlag("--verbose"));
}

// Test Adding an Option
TEST_F(CommandLineParserTest, AddOption)
{
    parser.addOption("--config", "Path to config file", true);
    EXPECT_EQ(parser.getOptionValue("--config"), "");
}

// Test Parsing a Flag
TEST_F(CommandLineParserTest, ParseFlag)
{
    const char* argv[] = { "program", "-debug" };
    int argc = sizeof(argv) / sizeof(argv[0]);

    parser.addFlag("debug", "Enable debug mode");
    parser.parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(parser.hasFlag("debug"));
}

// Test Parsing an Option with a Value
TEST_F(CommandLineParserTest, ParseOptionWithValue)
{
    const char* argv[] = { "program", "--config", "config.txt" };
    int argc = sizeof(argv) / sizeof(argv[0]);

    parser.addOption("-config", "Path to config file", true);
    parser.parse(argc, const_cast<char**>(argv));

    EXPECT_EQ(parser.getOptionValue("-config"), "config.txt");
}

// Test Parsing Multiple Arguments
TEST_F(CommandLineParserTest, ParseMultipleArguments)
{
    const char* argv[] = { "program", "--verbose", "--config", "settings.cfg" };
    int argc = sizeof(argv) / sizeof(argv[0]);

    parser.addFlag("-verbose", "Enable verbose logging");
    parser.addOption("-config", "Configuration file", true);
    parser.parse(argc, const_cast<char**>(argv));

    EXPECT_TRUE(parser.hasFlag("-verbose"));
    EXPECT_EQ(parser.getOptionValue("-config"), "settings.cfg");
}

// Test Parsing Unknown Arguments
TEST_F(CommandLineParserTest, ParseUnknownArguments)
{
    const char* argv[] = { "program", "--unknown" };
    int argc = sizeof(argv) / sizeof(argv[0]);

    parser.parse(argc, const_cast<char**>(argv));

    EXPECT_FALSE(parser.hasFlag("--unknown"));
}

// A second parse() replaces the first result instead of merging into it
TEST_F(CommandLineParserTest, ParseTwiceReplacesPreviousResult)
{
    parser.addFlag("v", "Verbose");
    parser.addOption("name", "Name", true);

    const char* first[] = { "program", "-v", "-name", "Alice" };
    parser.parse(4, const_cast<char**>(first));

    const char* second[] = { "program" };
    parser.parse(1, const_cast<char**>(second));

    EXPECT_FALSE(parser.hasFlag("v"));
    EXPECT_EQ(parser.getOptionValue("name"), "");
}

// Test Help Message Output (Redirecting stdout)
TEST_F(CommandLineParserTest, PrintHelp)
{
    testing::internal::CaptureStdout();
    parser.printHelp("test_program");
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("Usage: test_program [options]"), std::string::npos);
}
