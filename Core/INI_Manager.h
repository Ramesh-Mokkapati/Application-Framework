#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Logger.h"

using namespace Core::Framework;

namespace Core
{
    namespace Config
    {
        class INI_Manager
        {
        public:
            static INI_Manager& getInstance(Logger& logger)
            {
                static INI_Manager instance(logger);
                return instance;
            }

            INI_Manager(const INI_Manager&) = delete;
            INI_Manager& operator=(const INI_Manager&) = delete;

            // Replaces the current data with the file's contents.
            // If the file can't be opened, returns false and leaves the current data untouched.
            bool load(const string& filename);

            bool save(const string& filename) const;

            // An empty section name addresses the global (section-less) keys
            string get(const string& section, const string& key, const string& defaultValue = "") const;

            void set(const string& section, const string& key, const string& value);

            // Removes all globals, sections and keys
            void clear();

            // Sections whose name starts with prefix (a literal prefix, not a regex).
            // A single trailing '*' is accepted and ignored: "Task*" == "Task".
            vector<string> getSectionsMatching(const string& prefix) const;

            vector<string> getAllSections() const;

            map<string, map<string, string>> getAllData() const;

            map<string, map<string, string>> getAllDataWithGlobals() const;

        private:
            explicit INI_Manager(Logger& logger) : logger_(logger) {}

            map<string, string>					globalConfig_;          // Stores global parameters
            map<string, map<string, string>>	config_;                // Stores sectioned parameters
            vector<string>						sections_;              // Section names in file order, no duplicates
            mutable mutex						mutex_;
            Logger& logger_;

            static string trim(const string& str)
            {
                size_t first = str.find_first_not_of(" \t\r\n");
                size_t last = str.find_last_not_of(" \t\r\n");
                return (first == string::npos) ? "" : str.substr(first, last - first + 1);
            }
        };
    };
};
