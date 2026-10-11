// FeatureTour.cpp : Sample application built on the Core framework.
//
// Demonstrates: Application lifecycle, INI_Manager config (including app-specific defaults via
// addDefaultConfig()), CommandLineParser, Logger, LogParser, and the framework's exception
// handling in run().
//
// Everything the app shows on screen is also logged, together with the command line, the
// configuration in effect and where each setting came from, so the log alone explains a run.

#include "Application.h"
#include "CommandLineParser.h"
#include "LogParser.h"

#include <cctype>

using namespace Core::Framework;

class FeatureTourApp : public Application
{
public:
    FeatureTourApp(int argc, char* argv[]);

protected:
    void addDefaultConfig() override;
    void initialize() override;
    void execute() override;
    void cleanup() override;

private:
    void logCommandLine() const;
    void logConfiguration() const;
    int readRepeat() const;
    void runGreeter();
    void runTasks();
    void printLogReport();

    // Displays a line on screen and writes the same text to the log (INFO level)
    static void show(const string& line);

    // Accepts true/false, yes/no, on/off, 1/0 (any case). Returns false for anything else.
    static bool parseBool(const string& value, bool& result);

    CommandLineParser   m_cli;
    int                 m_argc;
    char**              m_argv;
    int                 m_nTasksRun = 0;
    int                 m_nTasksSkipped = 0;
};

FeatureTourApp::FeatureTourApp(int argc, char* argv[])
    : Application("FeatureTour")          // reads FeatureTour.ini, logs to FeatureTour.log
    , m_argc(argc)
    , m_argv(argv)
{
    // Register options first, then parse
    // (CommandLineParser(argc, argv) would parse before anything is registered)
    m_cli.addFlag("h", "Show this help");
    m_cli.addFlag("report", "Print a severity summary of FeatureTour.log at the end");
    m_cli.addFlag("fail", "Throw an exception to demonstrate Application::run() handling (exit code 1)");
    m_cli.addOption("name", "Name to greet (overrides [Greeter] name in the INI)", true);
}

// Without this, a missing FeatureTour.ini (first run, or the exe run from another folder) would be
// recreated with only [Logging] log_level = INFO: no DEBUG detail in the log, and no tasks to run.
// Keep in step with the FeatureTour.ini shipped next to the project.
void FeatureTourApp::addDefaultConfig()
{
    config.set("Logging", "log_level", "DEBUG");

    config.set("Greeter", "name", "World");
    config.set("Greeter", "repeat", "2");

    config.set("Task.Backup", "enabled", "true");
    config.set("Task.Backup", "message", "Backing up files...");

    config.set("Task.Cleanup", "enabled", "false");
    config.set("Task.Cleanup", "message", "Cleaning temp files...");

    config.set("Task.Report", "enabled", "true");
    config.set("Task.Report", "message", "Generating report...");
}

void FeatureTourApp::initialize()
{
    // Loads the INI (calling addDefaultConfig() if it's missing) and configures the logger
    Application::initialize();

    m_cli.parse(m_argc, m_argv);

    logger.log(LogLevel::INFO, "FeatureTour initialized");
    logCommandLine();
    logConfiguration();
}

void FeatureTourApp::execute()
{
    if (m_cli.hasFlag("h"))
    {
        logger.log(LogLevel::INFO, "Help requested (-h): showing usage and exiting");
        m_cli.printHelp("FeatureTour");
        return;
    }

    logger.log(LogLevel::INFO, "FeatureTour running");

    runGreeter();
    runTasks();

    logger.log(LogLevel::INFO, "Tasks: " + to_string(m_nTasksRun) + " run, " + to_string(m_nTasksSkipped) + " skipped");

    if (m_cli.hasFlag("fail"))
    {
        // Caught by Application::run(): logged as ERROR, echoed to stderr, cleanup() still
        // runs and run() returns 1
        throw runtime_error("Simulated failure requested via -fail");
    }

    if (m_cli.hasFlag("report"))
    {
        printLogReport();
    }
}

void FeatureTourApp::cleanup()
{
    logger.log(LogLevel::INFO, "FeatureTour shutting down");
    Application::cleanup();
}

// Records how the app was started: unknown options only reach stderr otherwise
void FeatureTourApp::logCommandLine() const
{
    string args;
    for (int i = 1; i < m_argc; ++i)
    {
        args += (i > 1 ? " " : "") + string(m_argv[i]);
    }

    logger.log(LogLevel::INFO, "Command line: " + (args.empty() ? string("(no arguments)") : args));
}

// Every section and key in effect for this run, in file order
void FeatureTourApp::logConfiguration() const
{
    const auto data = config.getAllData();

    for (const auto& section : config.getAllSections())
    {
        const auto it = data.find(section);
        if (it == data.end())
        {
            continue;
        }

        for (const auto& kv : it->second)
        {
            logger.log(LogLevel::DEBUG, "Config [" + section + "] " + kv.first + " = " + kv.second);
        }
    }
}

// [Greeter] repeat: a whole number of at least 1; anything else is logged and replaced by 1
int FeatureTourApp::readRepeat() const
{
    const string value = config.get("Greeter", "repeat", "1");

    try
    {
        size_t used = 0;
        const int repeat = stoi(value, &used);

        // stoi("2abc") would quietly give 2: require the whole value to be a number
        if (used == value.size() && repeat >= 1)
        {
            return repeat;
        }
    }
    catch (const exception&)
    {
        // Not a number (or out of range): reported below
    }

    logger.log(LogLevel::WARN, "Invalid [Greeter] repeat value '" + value + "' (expected a whole number >= 1), using 1");
    return 1;
}

// Command line value wins over INI, INI wins over the hard-coded default
void FeatureTourApp::runGreeter()
{
    string name = m_cli.getOptionValue("name");
    string source = "command line (-name)";

    if (name.empty())
    {
        const string fromIni = config.get("Greeter", "name");
        name = fromIni.empty() ? "World" : fromIni;
        source = fromIni.empty() ? "built-in default" : "[Greeter] name";
    }

    const int repeat = readRepeat();
    logger.log(LogLevel::DEBUG, "Greeting '" + name + "' (from " + source + ") " + to_string(repeat) + " time(s)");

    for (int i = 0; i < repeat; ++i)
    {
        show("Hello, " + name + "!");
    }
}

// Runs every [Task.*] section found in the INI
void FeatureTourApp::runTasks()
{
    // getSectionsMatching takes a literal prefix (not a regex)
    const vector<string> tasks = config.getSectionsMatching("Task.");
    logger.log(LogLevel::DEBUG, "Found " + to_string(tasks.size()) + " task section(s)");

    for (const auto& section : tasks)
    {
        const string enabledValue = config.get(section, "enabled", "true");

        bool enabled = false;
        if (!parseBool(enabledValue, enabled))
        {
            logger.log(LogLevel::WARN, "Skipping task " + section + ": invalid enabled value '" + enabledValue + "' (expected true or false)");
            ++m_nTasksSkipped;
            continue;
        }

        if (!enabled)
        {
            logger.log(LogLevel::INFO, "Skipping disabled task " + section);
            ++m_nTasksSkipped;
            continue;
        }

        show("[" + section + "] " + config.get(section, "message", "(no message)"));
        ++m_nTasksRun;
    }
}

// Reads the log file back with LogParser and counts entries per severity
void FeatureTourApp::printLogReport()
{
    try
    {
        // Same name Application::initialize() gives the log file
        LogParser parser(appName() + ".log");
        parser.parseLogs();

        map<string, int> counts;
        for (const auto& entry : parser.getAllLogs())
        {
            ++counts[entry.m_sSeverity];
        }

        cout << endl;
        show("Log summary (" + to_string(parser.getAllLogs().size()) + " entries):");
        for (const auto& kv : counts)
        {
            show("  " + kv.first + ": " + to_string(kv.second));
        }

        if (parser.getSkippedLineCount() > 0)
        {
            show("  (" + to_string(parser.getSkippedLineCount()) + " unparsable line(s) skipped)");
        }
    }
    catch (const exception& e)
    {
        logger.log(LogLevel::ERROR, string("Log report failed: ") + e.what());
    }
}

void FeatureTourApp::show(const string& line)
{
    cout << line << endl;
    logger.log(LogLevel::INFO, line);
}

bool FeatureTourApp::parseBool(const string& value, bool& result)
{
    string lower = value;
    transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });

    if (lower == "true" || lower == "yes" || lower == "on" || lower == "1")
    {
        result = true;
        return true;
    }

    if (lower == "false" || lower == "no" || lower == "off" || lower == "0")
    {
        result = false;
        return true;
    }

    return false;
}

int main(int argc, char* argv[])
{
    FeatureTourApp app(argc, argv);

    // initialize() -> execute() -> cleanup(); exceptions are handled inside.
    // Returns 0 on success, 1 on failure, so scripts can check the exit code.
    return app.run();
}
