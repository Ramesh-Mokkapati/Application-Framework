#include "INI_Manager.h"

namespace Core
{
    namespace Config
    {
        bool INI_Manager::load(const string& filename)
        {
            lock_guard<mutex> lock(mutex_);
            ifstream file(filename);
            if (!file.is_open())
            {
                cerr << "Warning: Cannot open INI file: " << filename << endl;
                return false;   // previously loaded data is left untouched
            }

            // Parse into temporaries so the live data is only replaced by a complete result
            map<string, string>					globalConfig;
            map<string, map<string, string>>	config;
            vector<string>						sections;

            string line, currentSection;
            bool firstLine = true;
            while (getline(file, line))
            {
                // Some editors save UTF-8 with a byte order mark; left in place it would hide
                // the first line (typically a "[Section]" header)
                if (firstLine && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
                {
                    line.erase(0, 3);
                }
                firstLine = false;

                line = trim(line);
                if (line.empty() || line[0] == ';' || line[0] == '#')
                {
                    continue;
                }

                if (line.front() == '[' && line.back() == ']')
                {
                    // New section detected (a repeated header continues the same section)
                    currentSection = trim(line.substr(1, line.size() - 2));

                    if (!currentSection.empty() &&
                        find(sections.begin(), sections.end(), currentSection) == sections.end())
                    {
                        sections.push_back(currentSection);
                        config.emplace(currentSection, map<string, string>());   // keep empty sections
                    }

                    continue;
                }

                const size_t pos = line.find('=');
                if (pos == string::npos)
                {
                    continue;
                }

                const string key = trim(line.substr(0, pos));
                if (key.empty())
                {
                    continue;
                }

                const string value = trim(line.substr(pos + 1));

                if (currentSection.empty())
                {
                    globalConfig[key] = value;  // Store global key-value pair
                }
                else
                {
                    config[currentSection][key] = value;
                }
            }

            globalConfig_ = move(globalConfig);
            config_ = move(config);
            sections_ = move(sections);

            return true;
        }

        string INI_Manager::get(const string& section, const string& key, const string& defaultValue) const
        {
            lock_guard<mutex> lock(mutex_);

            if (section.empty())
            {
                auto it = globalConfig_.find(key);
                return (it != globalConfig_.end()) ? it->second : defaultValue;
            }

            auto secIt = config_.find(section);
            if (secIt != config_.end())
            {
                auto keyIt = secIt->second.find(key);
                if (keyIt != secIt->second.end())
                {
                    return keyIt->second;
                }
            }

            return defaultValue;
        }

        map<string, map<string, string>> INI_Manager::getAllData() const
        {
            lock_guard<mutex> lock(mutex_);
            return config_;
        }

        map<string, map<string, string>> INI_Manager::getAllDataWithGlobals() const
        {
            lock_guard<mutex> lock(mutex_);
            map<string, map<string, string>> allData = config_; // Copy sectioned data

            // Include global parameters under a special key, e.g., "GLOBAL"
            if (!globalConfig_.empty())
            {
                allData["GLOBAL"] = globalConfig_;
            }

            return allData;
        }

        vector<string> INI_Manager::getAllSections() const
        {
            lock_guard<mutex> lock(mutex_);
            return sections_;
        }

        void INI_Manager::set(const string& section, const string& key, const string& value)
        {
            lock_guard<mutex> lock(mutex_);
            if (section.empty())
            {
                globalConfig_[key] = value;  // Set global parameter
            }
            else
            {
                if (find(sections_.begin(), sections_.end(), section) == sections_.end())
                {
                    sections_.push_back(section);
                }

                config_[section][key] = value;
            }
        }

        void INI_Manager::clear()
        {
            lock_guard<mutex> lock(mutex_);
            globalConfig_.clear();
            config_.clear();
            sections_.clear();
        }

        bool INI_Manager::save(const string& filename) const
        {
            lock_guard<mutex> lock(mutex_);
            ofstream file(filename);
            if (!file.is_open())
            {
                cerr << "Error: Cannot write to INI file: " << filename << endl;
                return false;
            }

            // Write global parameters first
            for (const auto& pair : globalConfig_)
            {
                file << pair.first << " = " << pair.second << "\n";
            }

            if (!globalConfig_.empty())
            {
                file << "\n";
            }

            // Write sections and their keys
            for (const auto& section : sections_)
            {
                file << "[" << section << "]\n";

                auto it = config_.find(section);
                if (it != config_.end())
                {
                    for (const auto& pair : it->second)
                    {
                        file << pair.first << " = " << pair.second << "\n";
                    }
                }

                file << "\n";
            }

            // Opening isn't enough: the writes themselves can fail (disk full, ...)
            file.flush();
            if (!file.good())
            {
                cerr << "Error: Failed writing INI file: " << filename << endl;
                return false;
            }

            return true;
        }

        vector<string> INI_Manager::getSectionsMatching(const string& prefix) const
        {
            lock_guard<mutex> lock(mutex_);

            string literal = prefix;
            if (!literal.empty() && literal.back() == '*')
            {
                literal.pop_back();
            }

            vector<string> matchingSections;
            for (const auto& section : sections_)
            {
                if (section.compare(0, literal.size(), literal) == 0)
                {
                    matchingSections.push_back(section);
                }
            }

            return matchingSections;
        }
    };
};
