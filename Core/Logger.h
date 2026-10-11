#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <iomanip>

using namespace std;

namespace Core
{
    namespace Framework
    {
        enum class LogLevel
        {
            STACK,
            DEBUG,
            INFO,
            WARN,
            ERROR,
            NONE // Used to disable logging
        };

        class Logger
        {
        public:
            static Logger& getInstance();

            // Opens filename in append mode (closing any previous file).
            // Prints a warning to stderr if it can't be opened; logging is then discarded.
            void setLogFile(const string& filename);

            void closeLogFile();

            void setLogLevel(LogLevel level);

            // Reads "log_level = <level>" (or "LogLevel = <level>") from a plain key=value file
            void loadConfig(const string& configFile);

            static void log(LogLevel level, const string& message);

            string levelToString(LogLevel level);

            // Case-insensitive, surrounding whitespace ignored ("debug", " INFO " are fine).
            // Returns false and leaves level unchanged if the name isn't a known level.
            static bool tryParseLevel(const string& levelStr, LogLevel& level);

            // Like tryParseLevel(), but an unknown name gives NONE
            LogLevel stringToLevel(const string& levelStr);

        protected:
            LogLevel		m_eCurrentLevel;
            ofstream		logFile_;
            mutex			logMutex_;

        private:
            Logger() : m_eCurrentLevel(LogLevel::INFO) {}
            ~Logger();

            Logger(const Logger&) = delete;
            Logger& operator=(const Logger&) = delete;

            string getTimestamp();
        };
    };
};
