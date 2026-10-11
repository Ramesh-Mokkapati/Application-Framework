#pragma once

#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace std;

namespace Core
{
    namespace Framework
    {
        struct LogEntry
        {
            string              m_sTimestamp;
            string              m_sSeverity;
            string              m_sMessage;
        };

        class LogParser
        {

        public:
            explicit LogParser(const string& filePath);

            // Reads the file and replaces any previously parsed entries, so it is safe to call repeatedly.
            // Entry format: "YYYY-MM-DD HH:MM:SS[.mmm] [SEVERITY] message"
            // A line that doesn't start a new entry (stack trace, multi-line message) is appended
            // to the previous entry's message, separated by '\n'. Blank lines are ignored.
            // Non-blank lines before the first entry are counted by getSkippedLineCount().
            // Throws runtime_error if the file can't be opened.
            void parseLogs();

            vector<LogEntry> filterBySeverity(const string& severity) const;
            vector<LogEntry> getAllLogs() const;
            size_t getSkippedLineCount() const;

        private:
            string				m_sFilePath;
            vector<LogEntry>	m_vLogs;
            size_t				m_nSkippedLines = 0;
        };
    };
};
