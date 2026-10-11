#include "LogParser.h"

namespace Core
{
    namespace Framework
    {
        LogParser::LogParser(const string& filePath)
            : m_sFilePath(filePath)
        {
        }

        void LogParser::parseLogs()
        {
            m_vLogs.clear();
            m_nSkippedLines = 0;

            ifstream file(m_sFilePath);
            if (!file.is_open())
            {
                throw runtime_error("Failed to open log file: " + m_sFilePath);
            }

            // Milliseconds are optional: Logger writes "%Y-%m-%d %H:%M:%S" without them
            static const regex logPattern(R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d{3})?) \[(\w+)\] (.*))");

            string line;
            smatch match;

            while (getline(file, line))
            {
                if (regex_match(line, match, logPattern))
                {
                    LogEntry entry
                    {
                        match[1].str(), // timestamp
                        match[2].str(), // severity
                        match[3].str()  // message
                    };

                    m_vLogs.push_back(entry);
                }
                else if (line.empty())
                {
                    continue;
                }
                else if (!m_vLogs.empty())
                {
                    // Continuation of the previous entry (stack trace, multi-line message)
                    m_vLogs.back().m_sMessage += "\n" + line;
                }
                else
                {
                    ++m_nSkippedLines;
                }
            }
        }

        vector<LogEntry> LogParser::filterBySeverity(const string& severity) const
        {
            vector<LogEntry> filteredLogs;
            for (const auto& log : m_vLogs)
            {
                if (log.m_sSeverity == severity)
                {
                    filteredLogs.push_back(log);
                }
            }

            return filteredLogs;
        }

        vector<LogEntry> LogParser::getAllLogs() const
        {
            return m_vLogs;
        }

        size_t LogParser::getSkippedLineCount() const
        {
            return m_nSkippedLines;
        }
    };
};
