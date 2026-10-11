#include "pch.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "Application.h"
#include "INI_Manager.h"

using namespace Core::Framework;
using namespace Core::Config;

namespace
{
    void writeFile(const std::string& path, const std::string& content)
    {
        std::ofstream file(path, std::ios::trunc);
        file << content;
    }

    std::string readFile(const std::string& path)
    {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // INI_Manager is a singleton: clear it so state left by earlier tests can't leak in.
    void resetConfig()
    {
        INI_Manager::getInstance(Logger::getInstance()).clear();
    }

    // Runs the application and returns whatever it wrote to stderr
    // (INI_Manager and Application::run report problems there), keeping the test output clean.
    // The exit code from run() is returned through exitCode when provided.
    std::string runQuietly(Application& app, int* exitCode = nullptr)
    {
        testing::internal::CaptureStderr();
        int result = 0;
        try
        {
            result = app.run();
        }
        catch (...)
        {
            testing::internal::GetCapturedStderr();
            throw;
        }

        if (exitCode)
        {
            *exitCode = result;
        }

        return testing::internal::GetCapturedStderr();
    }

    // Concrete Application that records what the framework does to it.
    class TestApplication : public Application
    {
    public:
        explicit TestApplication(const std::string& name, const std::string& iniFile = "")
            : Application(name, iniFile)
        {
            // Tests run unattended: no summary on stdout, and never wait for Enter
            setExitPrompt(false);
        }

        // Behaviour switches
        bool throwStd = false;
        bool throwUnknown = false;
        bool throwInInitialize = false;
        bool throwInCleanup = false;
        bool addDefaults = false;       // addDefaultConfig() adds [App] name and log_level DEBUG
        std::string errorMessage = "test failure";

        // Observations
        std::vector<std::string> calls;
        int addDefaultConfigCalls = 0;
        std::string observedName = "<not read>";

    protected:
        void addDefaultConfig() override
        {
            ++addDefaultConfigCalls;

            if (addDefaults)
            {
                config.set("App", "name", "FromDefaults");
                config.set("Logging", "log_level", "DEBUG");
            }
        }

        void initialize() override
        {
            calls.push_back("initialize");
            Application::initialize();

            if (throwInInitialize)
            {
                throw std::runtime_error(errorMessage);
            }
        }

        void execute() override
        {
            calls.push_back("execute");
            observedName = config.get("App", "name", "<unset>");

            if (throwStd)
            {
                throw std::runtime_error(errorMessage);
            }

            if (throwUnknown)
            {
                throw 42;
            }
        }

        void cleanup() override
        {
            calls.push_back("cleanup");
            Application::cleanup();

            if (throwInCleanup)
            {
                throw std::runtime_error(errorMessage);
            }
        }
    };
}

// ---------------------------------------------------------------------------
// Lifecycle, exit code, exception handling and config loading
// ---------------------------------------------------------------------------

class ApplicationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        resetConfig();
        std::remove(defaultIni.c_str());
        std::remove(customIni.c_str());
    }

    void TearDown() override
    {
        std::remove(defaultIni.c_str());
        std::remove(customIni.c_str());
    }

    const std::string appName = "ApplicationTestApp";
    const std::string defaultIni = "ApplicationTestApp.ini";   // <appName>.ini
    const std::string customIni = "ApplicationTestCustom.ini";
};

TEST_F(ApplicationTest, IsAbstract)
{
    EXPECT_TRUE(std::is_abstract<Application>::value);
}

TEST_F(ApplicationTest, RunCallsLifecycleInOrder)
{
    TestApplication app(appName);
    runQuietly(app);

    const std::vector<std::string> expected = { "initialize", "execute", "cleanup" };
    EXPECT_EQ(app.calls, expected);
}

TEST_F(ApplicationTest, RunReturnsZeroOnSuccess)
{
    TestApplication app(appName);

    int exitCode = -1;
    runQuietly(app, &exitCode);

    EXPECT_EQ(exitCode, 0);
}

TEST_F(ApplicationTest, StdExceptionInExecuteDoesNotEscapeRun)
{
    TestApplication app(appName);
    app.throwStd = true;

    EXPECT_NO_THROW(runQuietly(app));
}

TEST_F(ApplicationTest, UnknownExceptionInExecuteDoesNotEscapeRun)
{
    TestApplication app(appName);
    app.throwUnknown = true;

    EXPECT_NO_THROW(runQuietly(app));
}

TEST_F(ApplicationTest, RunReturnsOneWhenExecuteThrowsStdException)
{
    TestApplication app(appName);
    app.throwStd = true;

    int exitCode = -1;
    runQuietly(app, &exitCode);

    EXPECT_EQ(exitCode, 1);
}

TEST_F(ApplicationTest, RunReturnsOneWhenExecuteThrowsUnknownException)
{
    TestApplication app(appName);
    app.throwUnknown = true;

    int exitCode = -1;
    runQuietly(app, &exitCode);

    EXPECT_EQ(exitCode, 1);
}

TEST_F(ApplicationTest, CleanupRunsAfterExceptionInExecute)
{
    TestApplication app(appName);
    app.throwStd = true;
    runQuietly(app);

    const std::vector<std::string> expected = { "initialize", "execute", "cleanup" };
    EXPECT_EQ(app.calls, expected);
}

TEST_F(ApplicationTest, ExceptionInInitializeSkipsExecuteButRunsCleanup)
{
    TestApplication app(appName);
    app.throwInInitialize = true;

    int exitCode = -1;
    EXPECT_NO_THROW(runQuietly(app, &exitCode));

    const std::vector<std::string> expected = { "initialize", "cleanup" };
    EXPECT_EQ(app.calls, expected);
    EXPECT_EQ(exitCode, 1);
}

TEST_F(ApplicationTest, ExceptionInCleanupDoesNotEscapeRunAndReturnsOne)
{
    TestApplication app(appName);
    app.throwInCleanup = true;

    int exitCode = -1;
    EXPECT_NO_THROW(runQuietly(app, &exitCode));

    EXPECT_EQ(exitCode, 1);
}

TEST_F(ApplicationTest, FatalErrorsAreEchoedToStderr)
{
    TestApplication app(appName);
    app.throwStd = true;
    app.errorMessage = "stderr-marker";

    const std::string err = runQuietly(app);

    EXPECT_NE(err.find("Unhandled exception: stderr-marker"), std::string::npos);
}

TEST_F(ApplicationTest, LoadsConfigFromDefaultIniName)
{
    writeFile(defaultIni, "[App]\nname = FromDefaultIni\n");

    TestApplication app(appName);
    runQuietly(app);

    EXPECT_EQ(app.observedName, "FromDefaultIni");
}

TEST_F(ApplicationTest, LoadsConfigFromCustomIniPath)
{
    writeFile(defaultIni, "[App]\nname = Wrong\n");
    writeFile(customIni, "[App]\nname = Right\n");

    TestApplication app(appName, customIni);
    runQuietly(app);

    EXPECT_EQ(app.observedName, "Right");
}

TEST_F(ApplicationTest, MissingIniFileFallsBackToDefaults)
{
    TestApplication app(appName);

    std::string err;
    EXPECT_NO_THROW(err = runQuietly(app));

    EXPECT_EQ(app.observedName, "<unset>");
    EXPECT_NE(err.find("Warning: Cannot open INI file"), std::string::npos);
    EXPECT_NE(err.find(defaultIni), std::string::npos);
    EXPECT_NE(err.find("creating one with defaults"), std::string::npos);
}

TEST_F(ApplicationTest, MissingIniFileStillRunsFullLifecycle)
{
    TestApplication app(appName);

    int exitCode = -1;
    runQuietly(app, &exitCode);

    const std::vector<std::string> expected = { "initialize", "execute", "cleanup" };
    EXPECT_EQ(app.calls, expected);
    EXPECT_EQ(exitCode, 0);   // a missing INI is a warning, not a failure
}

TEST_F(ApplicationTest, MissingIniFileIsCreatedWithDefaultLogLevel)
{
    ASSERT_FALSE(std::ifstream(defaultIni).good());

    TestApplication app(appName);
    runQuietly(app);

    const std::string ini = readFile(defaultIni);
    EXPECT_NE(ini.find("[Logging]"), std::string::npos);
    EXPECT_NE(ini.find("log_level = INFO"), std::string::npos);
}

TEST_F(ApplicationTest, CreatedIniIsUsedByTheNextRun)
{
    TestApplication first(appName);
    runQuietly(first);               // creates the INI file

    resetConfig();                   // forget in-memory state, as a fresh process would
    TestApplication second(appName);
    const std::string err = runQuietly(second);

    EXPECT_EQ(err.find("Cannot open INI file"), std::string::npos);
}

TEST_F(ApplicationTest, ExistingIniFileIsNotOverwritten)
{
    const std::string original = "[App]\nname = Keep\n";
    writeFile(defaultIni, original);

    TestApplication app(appName);
    runQuietly(app);

    EXPECT_EQ(readFile(defaultIni), original);
    EXPECT_EQ(app.observedName, "Keep");
}

TEST_F(ApplicationTest, AddDefaultConfigFillsMissingIniFile)
{
    TestApplication app(appName);
    app.addDefaults = true;
    runQuietly(app);

    EXPECT_EQ(app.addDefaultConfigCalls, 1);

    // Used for this run...
    EXPECT_EQ(app.observedName, "FromDefaults");

    // ...and written to the new file, alongside (and able to override) the default [Logging]
    const std::string ini = readFile(defaultIni);
    EXPECT_NE(ini.find("name = FromDefaults"), std::string::npos);
    EXPECT_NE(ini.find("log_level = DEBUG"), std::string::npos);
    EXPECT_EQ(ini.find("log_level = INFO"), std::string::npos);
}

TEST_F(ApplicationTest, AddDefaultConfigIsNotCalledWhenIniExists)
{
    writeFile(defaultIni, "[App]\nname = Keep\n");

    TestApplication app(appName);
    app.addDefaults = true;
    runQuietly(app);

    EXPECT_EQ(app.observedName, "Keep");
    EXPECT_EQ(app.addDefaultConfigCalls, 0);
}

TEST_F(ApplicationTest, CustomIniPathIsCreatedWhenMissing)
{
    TestApplication app(appName, customIni);
    runQuietly(app);

    EXPECT_NE(readFile(customIni).find("log_level = INFO"), std::string::npos);
    EXPECT_FALSE(std::ifstream(defaultIni).good());
}

TEST_F(ApplicationTest, SecondApplicationSeesItsOwnConfig)
{
    writeFile(defaultIni, "[App]\nname = First\n");
    writeFile(customIni, "[App]\nname = Second\n");

    TestApplication first(appName);
    runQuietly(first);

    TestApplication second("ApplicationTestSecond", customIni);
    runQuietly(second);

    EXPECT_EQ(first.observedName, "First");
    EXPECT_EQ(second.observedName, "Second");
}

// ---------------------------------------------------------------------------
// Logging behaviour
//
// These tests read the application's log file, so they rely on
// Logger::setLogFile closing the previously opened file before opening a new one.
// Each test uses its own app name, so it has its own log and INI file.
// ---------------------------------------------------------------------------

class ApplicationLoggingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        resetConfig();

        const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        appName = "ApplicationLoggingTest_" + testName;
        iniFile = appName + ".ini";
        logFile = appName + ".log";

        std::remove(iniFile.c_str());
        std::remove(logFile.c_str());
    }

    void TearDown() override
    {
        std::remove(iniFile.c_str());
        std::remove(logFile.c_str());   // may fail while the logger still holds it open
    }

    std::string appName;
    std::string iniFile;
    std::string logFile;
};

TEST_F(ApplicationLoggingTest, StdExceptionIsLoggedAsError)
{
    writeFile(iniFile, "[Logging]\nlog_level = ERROR\n");

    TestApplication app(appName);
    app.throwStd = true;
    app.errorMessage = appName;     // unique marker to search for
    runQuietly(app);

    const std::string log = readFile(logFile);
    EXPECT_NE(log.find("[ERROR]"), std::string::npos);
    EXPECT_NE(log.find("Unhandled exception: " + appName), std::string::npos);
}

TEST_F(ApplicationLoggingTest, UnknownExceptionIsLoggedAsError)
{
    writeFile(iniFile, "[Logging]\nlog_level = ERROR\n");

    TestApplication app(appName);
    app.throwUnknown = true;
    runQuietly(app);

    const std::string log = readFile(logFile);
    EXPECT_NE(log.find("[ERROR]"), std::string::npos);
    EXPECT_NE(log.find("Unhandled unknown exception occurred."), std::string::npos);
}

TEST_F(ApplicationLoggingTest, CleanupExceptionIsLoggedAsError)
{
    writeFile(iniFile, "[Logging]\nlog_level = ERROR\n");

    TestApplication app(appName);
    app.throwInCleanup = true;
    app.errorMessage = appName;
    runQuietly(app);

    const std::string log = readFile(logFile);
    EXPECT_NE(log.find("Exception during cleanup: " + appName), std::string::npos);
}

TEST_F(ApplicationLoggingTest, LogLevelFromIniFiltersLowerLevels)
{
    writeFile(iniFile, "[Logging]\nlog_level = ERROR\n");

    TestApplication app(appName);
    app.throwStd = true;            // guarantees at least one ERROR line
    runQuietly(app);

    const std::string log = readFile(logFile);
    EXPECT_NE(log.find("[ERROR]"), std::string::npos);
    EXPECT_EQ(log.find("[DEBUG]"), std::string::npos);
    EXPECT_EQ(log.find("[INFO]"), std::string::npos);
}

TEST_F(ApplicationLoggingTest, DebugLevelFromIniLogsLifecycle)
{
    writeFile(iniFile, "[Logging]\nlog_level = DEBUG\n");

    TestApplication app(appName);
    runQuietly(app);

    const std::string log = readFile(logFile);
    EXPECT_NE(log.find("[DEBUG]"), std::string::npos);
    EXPECT_NE(log.find("initialize"), std::string::npos);
    EXPECT_NE(log.find("cleanup"), std::string::npos);
}

TEST_F(ApplicationLoggingTest, LogLevelIsCaseInsensitive)
{
    writeFile(iniFile, "[Logging]\nlog_level = debug\n");

    TestApplication app(appName);
    const std::string err = runQuietly(app);

    EXPECT_NE(readFile(logFile).find("[DEBUG]"), std::string::npos);
    EXPECT_EQ(err.find("Unknown log_level"), std::string::npos);
}

TEST_F(ApplicationLoggingTest, UnknownLogLevelWarnsAndDisablesLogging)
{
    writeFile(iniFile, "[Logging]\nlog_level = verbose\n");

    TestApplication app(appName);
    app.throwStd = true;
    app.errorMessage = appName;
    const std::string err = runQuietly(app);

    EXPECT_NE(err.find("Warning: Unknown log_level 'verbose'"), std::string::npos);
    EXPECT_EQ(readFile(logFile).find(appName), std::string::npos);
}

TEST_F(ApplicationLoggingTest, LogLevelMissingFromExistingIniDefaultsToNone)
{
    // The INI exists but has no [Logging] section: log_level falls back to NONE
    // and nothing is written to the log
    writeFile(iniFile, "[App]\nname = X\n");

    TestApplication app(appName);
    app.throwStd = true;
    app.errorMessage = appName;
    runQuietly(app);

    EXPECT_TRUE(std::ifstream(logFile).good()) << "Log file should still be created";
    EXPECT_EQ(readFile(logFile).find(appName), std::string::npos);
}

TEST_F(ApplicationLoggingTest, MissingIniFileUsesInfoLevelAndLogsCreation)
{
    // No INI at all: a default one is created, so logging works out of the box
    TestApplication app(appName);
    app.throwStd = true;
    app.errorMessage = appName;
    runQuietly(app);

    const std::string log = readFile(logFile);
    EXPECT_NE(log.find("[INFO]"), std::string::npos);
    EXPECT_NE(log.find("created it with default settings (log_level = INFO)"), std::string::npos);
    EXPECT_NE(log.find("Unhandled exception: " + appName), std::string::npos);
    EXPECT_EQ(log.find("[DEBUG]"), std::string::npos);
}

TEST_F(ApplicationLoggingTest, LoadedIniFileIsLogged)
{
    writeFile(iniFile, "[Logging]\nlog_level = info\n");

    TestApplication app(appName);
    runQuietly(app);

    EXPECT_NE(readFile(logFile).find("Loaded settings from '" + iniFile + "' (log_level = INFO)"), std::string::npos);
}
