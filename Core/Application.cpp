#include "Application.h"

#include <filesystem>
#include <cstdio>
#include <io.h>

namespace Core
{
    namespace Framework
    {
        // Log level written to a newly created INI file (and used for that run)
        static const char* const kDefaultLogLevel = "INFO";

        // Define static members outside the class
        Logger& Application::logger = Logger::getInstance();

        Application::Application(const string& appName, const string& iniFile)
            : config(Config::INI_Manager::getInstance(Logger::getInstance()))
            , m_sAppName(appName)
            , m_sIniFile(iniFile.empty() ? appName + ".ini" : iniFile)
        {
        }

        Application::~Application()
        {
            logger.log(LogLevel::DEBUG, __FUNCTION__);
        }

        int Application::run()
        {
            logger.log(LogLevel::DEBUG, __FUNCTION__);

            int exitCode = 0;

            try
            {
                initialize();
                execute();
            }
            catch (const exception& e)
            {
                exitCode = 1;
                reportError("Unhandled exception: " + string(e.what()));
            }
            catch (...)
            {
                exitCode = 1;
                reportError("Unhandled unknown exception occurred.");
            }

            // cleanup() always runs, and a failure in it must not escape run() either
            try
            {
                cleanup();
            }
            catch (const exception& e)
            {
                exitCode = 1;
                reportError("Exception during cleanup: " + string(e.what()));
            }
            catch (...)
            {
                exitCode = 1;
                reportError("Unknown exception during cleanup.");
            }

            showExitPrompt(exitCode);

            return exitCode;
        }

        void Application::initialize()
        {
            // Load config first so the log level/file can come from it.
            // No INI file yet (e.g. first run): write one with default logging settings so there is
            // something to edit. If the existence check itself fails, load() reports the problem.
            error_code ec;
            const bool iniMissing = !filesystem::exists(m_sIniFile, ec) && !ec;

            bool configLoaded = false;
            bool defaultsCreated = false;

            if (iniMissing)
            {
                cerr << "Warning: Cannot open INI file: " << m_sIniFile << ", creating one with defaults" << endl;
                defaultsCreated = createDefaultConfig();
            }
            else
            {
                configLoaded = config.load(m_sIniFile);
            }

            // Section "Logging", key "log_level" (defaults to NONE; level names are case-insensitive)
            const string levelName = config.get("Logging", "log_level", "NONE");
            LogLevel level = LogLevel::NONE;
            if (!Logger::tryParseLevel(levelName, level))
            {
                // A typo would otherwise silently turn logging off
                cerr << "Warning: Unknown log_level '" << levelName << "' in " << m_sIniFile
                     << " (expected STACK, DEBUG, INFO, WARN, ERROR or NONE); logging is off" << endl;
            }

            logger.setLogLevel(level);
            logger.setLogFile(m_sAppName + ".log");

            logger.log(LogLevel::DEBUG, __FUNCTION__);

            // The level actually in effect (addDefaultConfig() may have changed the default)
            const string levelInEffect = logger.levelToString(level);

            if (defaultsCreated)
            {
                logger.log(LogLevel::INFO, "INI file '" + m_sIniFile + "' not found; created it with default settings (log_level = " + levelInEffect + ").");
            }
            else if (configLoaded)
            {
                logger.log(LogLevel::INFO, "Loaded settings from '" + m_sIniFile + "' (log_level = " + levelInEffect + ").");
            }
            else
            {
                logger.log(LogLevel::WARN, "Could not load INI file '" + m_sIniFile + "', using defaults.");
            }
        }

        void Application::cleanup()
        {
            logger.log(LogLevel::DEBUG, __FUNCTION__);
        }

        bool Application::createDefaultConfig()
        {
            // The file exists but couldn't be read (permissions, locked, ...): don't overwrite it
            error_code ec;
            if (filesystem::exists(m_sIniFile, ec))
            {
                return false;
            }

            // Start from a clean slate so nothing stale from an earlier load is written out
            config.clear();
            config.set("Logging", "log_level", kDefaultLogLevel);
            addDefaultConfig();

            return config.save(m_sIniFile);
        }

        void Application::showExitPrompt(int exitCode) const
        {
            if (!m_bExitPrompt)
            {
                return;
            }

            cout << endl << m_sAppName
                 << (exitCode == 0 ? " finished successfully" : " finished with errors")
                 << " (exit code " << exitCode << ")." << endl;

            // Only wait in an interactive console: with redirected stdin/stdout nobody can press Enter
            if (_isatty(_fileno(stdin)) && _isatty(_fileno(stdout)))
            {
                cout << "Press Enter to exit..." << endl;
                cin.get();
            }
        }

        void Application::reportError(const string& message)
        {
            logger.log(LogLevel::ERROR, message);
            cerr << "Error: " << message << endl;
        }
    };
};
