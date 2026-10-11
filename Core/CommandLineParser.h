#pragma once

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>

using namespace std;

namespace Core
{
    namespace Framework
    {
        class CommandLineParser
        {
        public:
            CommandLineParser() = default;

            CommandLineParser(int argc, char* argv[])
            {
                parse(argc, argv);
            }

            void addOption(const string& name, const string& description, bool requiresValue = false);

            void addFlag(const string& name, const string& description);

            void parse(int argc, char* argv[]);

            bool hasFlag(const string& name) const;

            string getOptionValue(const string& name) const;

            void printHelp(const string& appName) const;

        private:
            struct Option
            {
                string      m_sDescription;
                bool        m_bRequiresValue;
                string      m_sValue;
                bool        m_bIsSet = false;
            };

            map<string, Option>     m_mOptions;
            vector<string>          m_vPositionalArgs;
        };
    };
};

