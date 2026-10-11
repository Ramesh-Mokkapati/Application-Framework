#include "CommandLineParser.h"

#include <iostream>
#include <sstream>

namespace Core
{
    namespace Framework
    {
        // Add an option (expects a value)
        void CommandLineParser::addOption(const std::string& name, const std::string& description, bool requiresValue)
        {
            m_mOptions[name] = { description, requiresValue, "", false };
        }

        // Add a flag (doesn't expect a value)
        void CommandLineParser::addFlag(const std::string& name, const std::string& description)
        {
            m_mOptions[name] = { description, false, "", false };
        }

        void CommandLineParser::parse(int argc, char* argv[])
        {
            // Each parse() replaces the previous result instead of merging into it
            for (auto& opt : m_mOptions)
            {
                opt.second.m_bIsSet = false;
                opt.second.m_sValue.clear();
            }
            m_vPositionalArgs.clear();

            for (int i = 1; i < argc; ++i)
            {
                std::string arg = argv[i];

                if (arg[0] == '-')
                {
                    std::string optName = arg.substr(1); // Remove leading '-'

                    auto it = m_mOptions.find(optName);
                    if (it != m_mOptions.end())
                    {
                        Option& opt = it->second;
                        opt.m_bIsSet = true;

                        if (opt.m_bRequiresValue)
                        {
                            if (i + 1 < argc)
                            {
                                std::string nextArg = argv[i + 1];

                                // Ensure the next argument is NOT another option
                                if (nextArg[0] != '-')
                                {
                                    opt.m_sValue = nextArg;
                                    ++i; // Skip the value argument
                                }
                                else
                                {
                                    std::cerr << "Error: Option -" << optName << " requires a value.\n";
                                }
                            }
                            else
                            {
                                std::cerr << "Error: Missing value for option -" << optName << ".\n";
                            }
                        }
                    }
                    else
                    {
                        std::cerr << "Warning: Unknown option -" << optName << "\n";
                    }
                }
                else
                {
                    m_vPositionalArgs.push_back(arg);
                }
            }
        }


        // Check if a flag was set
        bool CommandLineParser::hasFlag(const std::string& name) const
        {
            auto it = m_mOptions.find(name);
            return it != m_mOptions.end() && it->second.m_bIsSet;
        }

        // Get the value of an option
        std::string CommandLineParser::getOptionValue(const std::string& name) const
        {
            auto it = m_mOptions.find(name);
            return (it != m_mOptions.end() && it->second.m_bIsSet) ? it->second.m_sValue : "";
        }

        // Print help message
        void CommandLineParser::printHelp(const std::string& appName) const
        {
            std::cout << "Usage: " << appName << " [options]\n";
            for (const auto& opt : m_mOptions)
            {
                std::cout << "  -" << opt.first << " : " << opt.second.m_sDescription;
                if (opt.second.m_bRequiresValue)
                {
                    std::cout << " (requires value)";
                }

                std::cout << "\n";
            }
        }
    };
};
