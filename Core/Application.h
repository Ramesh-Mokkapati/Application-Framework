#pragma once

#include <iostream>
#include <string>
#include <exception>
#include <algorithm>

#include "Logger.h"
#include "INI_Manager.h"

using namespace std;

namespace Core
{
    namespace Framework
    {
        class Application
        {
        public:
            // iniFile defaults to "<appName>.ini" when left empty.
            // If the INI file doesn't exist, initialize() creates it with a default [Logging] section.
            explicit Application(const string& appName, const string& iniFile = "");
            virtual ~Application();

            Application(const Application&) = delete;
            Application& operator=(const Application&) = delete;

            // Runs initialize() -> execute() -> cleanup(). Exceptions never escape.
            // Returns 0 on success, 1 if initialize(), execute() or cleanup() threw.
            // Then, if the exit prompt is enabled, prints the result and waits for Enter.
            int run();

            // On by default: at the end of run(), print "<appName> finished ..." with the result,
            // then "Press Enter to exit..." and wait. The wait only happens in an interactive
            // console, so piped/scripted runs don't hang. Turn it off for automated runs (tests).
            void setExitPrompt(bool enabled) { m_bExitPrompt = enabled; }

        protected:
            virtual void initialize();
            virtual void execute() = 0; // Pure virtual function for application logic
            virtual void cleanup();

            // Called by initialize() when the INI file doesn't exist, after the default
            // [Logging] log_level has been set and before the file is written. Override to add
            // the application's own default settings with config.set(), so a first run (or a run
            // from another folder) gets a complete INI file. Settings set here are also used
            // for the current run, even if the file can't be written.
            virtual void addDefaultConfig() {}

            const string& appName() const { return m_sAppName; }

            static Logger& logger;
            Config::INI_Manager& config;   // Available to derived classes

        private:
            // Logs at ERROR level and echoes to stderr, so failures stay visible
            // even when the configured log level is NONE
            static void reportError(const string& message);

            // Used when the INI file doesn't exist: resets the in-memory config to the default
            // logging settings plus addDefaultConfig(), and writes them to m_sIniFile.
            // Returns true if the file was created.
            // Does nothing (returns false) if the file exists but couldn't be read.
            bool createDefaultConfig();

            void showExitPrompt(int exitCode) const;

            string						m_sAppName;
            string						m_sIniFile;
            bool						m_bExitPrompt = true;
        };
    };
};
