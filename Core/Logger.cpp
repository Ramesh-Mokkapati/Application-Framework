#include "Logger.h"

#include <algorithm>
#include <cctype>

namespace Core
{
    namespace Framework
    {
        static string trimmed(const string& str)
        {
            const size_t first = str.find_first_not_of(" \t\r\n");
            const size_t last = str.find_last_not_of(" \t\r\n");
            return (first == string::npos) ? "" : str.substr(first, last - first + 1);
        }

        Logger& Logger::getInstance()
        {
            static Logger instance;
            return instance;
        }

        void Logger::setLogFile(const string& filename)
        {
            lock_guard<mutex> lock(logMutex_);
            if (logFile_.is_open())
            {
                logFile_.close();
            }

            logFile_.clear();
            logFile_.open(filename, ios::app);

            // Otherwise every later log() call is silently dropped
            if (!logFile_.is_open())
            {
                cerr << "Warning: Cannot open log file: " << filename << endl;
            }
        }

        void Logger::closeLogFile()
        {
            lock_guard<mutex> lock(logMutex_);
            if (logFile_.is_open())
            {
                logFile_.close();
            }
        }

        void Logger::setLogLevel(LogLevel level)
        {
            lock_guard<mutex> lock(logMutex_);
            m_eCurrentLevel = level;
        }

        void Logger::loadConfig(const string& configFile)
        {
            lock_guard<mutex> lock(logMutex_);
            ifstream file(configFile);
            string line;
            while (getline(file, line))
            {
                const size_t pos = line.find('=');
                if (pos == string::npos)
                {
                    continue;
                }

                // Exact key match: a line merely containing "LogLevel" (e.g. a comment) is ignored
                const string key = trimmed(line.substr(0, pos));
                if (key == "log_level" || key == "LogLevel")
                {
                    m_eCurrentLevel = stringToLevel(line.substr(pos + 1));
                }
            }
        }

        void Logger::log(LogLevel level, const string& message)
        {
            Logger& logger = getInstance();
            lock_guard<mutex> lock(logger.logMutex_);
            if (level >= logger.m_eCurrentLevel)
            {
                string logMessage = logger.getTimestamp() + " [" + logger.levelToString(level) + "] " + message;
                //cout << logMessage << endl;
                if (logger.logFile_.is_open())
                {
                    logger.logFile_ << logMessage << endl;
                    logger.logFile_.flush(); // Ensure log is written immediately
                }
            }
        }

        Logger::~Logger()
        {
            if (logFile_.is_open())
            {
                logFile_.close();
            }
        }

        string Logger::getTimestamp()
        {
            time_t now = time(0);
            struct tm ltm;
            localtime_s(&ltm, &now);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &ltm);
            return string(buffer);
        }

        string Logger::levelToString(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::DEBUG:
                return "DEBUG";

            case LogLevel::INFO:
                return "INFO";

            case LogLevel::WARN:
                return "WARN";

            case LogLevel::ERROR:
                return "ERROR";

            case LogLevel::STACK:
                return "STACK";

            case LogLevel::NONE:
                return "NONE";
            }

            return "UNKNOWN";
        }

        bool Logger::tryParseLevel(const string& levelStr, LogLevel& level)
        {
            string name = trimmed(levelStr);
            transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) { return static_cast<char>(toupper(c)); });

            static const map<string, LogLevel> levels =
            {
                { "STACK", LogLevel::STACK },
                { "DEBUG", LogLevel::DEBUG },
                { "INFO",  LogLevel::INFO },
                { "WARN",  LogLevel::WARN },
                { "ERROR", LogLevel::ERROR },
                { "NONE",  LogLevel::NONE },
            };

            const auto it = levels.find(name);
            if (it == levels.end())
            {
                return false;
            }

            level = it->second;
            return true;
        }

        LogLevel Logger::stringToLevel(const string& levelStr)
        {
            LogLevel level = LogLevel::NONE;    // Default for unknown names
            tryParseLevel(levelStr, level);
            return level;
        }
    };
};
