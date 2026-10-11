# Application-Framework

A small C++ framework for Windows console applications. Derive from `Application`, implement
`execute()`, and the framework handles the rest of the lifecycle: INI configuration (created with
defaults on first run), file logging, exception handling, exit codes and a final
"Press Enter to exit" prompt.

```cpp
#include "Application.h"

using namespace Core::Framework;

class HelloApp : public Application
{
public:
    HelloApp() : Application("HelloApp") {}     // reads HelloApp.ini, logs to HelloApp.log

protected:
    void execute() override
    {
        cout << "Hello, " << config.get("Greeter", "name", "World") << "!" << endl;
        logger.log(LogLevel::INFO, "Greeted");
    }
};

int main()
{
    HelloApp app;
    return app.run();       // 0 on success, 1 on failure
}
```

## Contents

- [Repository layout](#repository-layout)
- [Building](#building)
- [The Core library](#the-core-library)
  - [Application](#application)
  - [INI_Manager](#ini_manager)
  - [Logger](#logger)
  - [LogParser](#logparser)
  - [CommandLineParser](#commandlineparser)
- [Sample applications](#sample-applications)
  - [FeatureTour](#featuretour)
  - [SingleInstance](#singleinstance)
  - [NetworkAdapters](#networkadapters)
- [Tests](#tests)
- [Continuous integration](#continuous-integration)
- [Creating a new application](#creating-a-new-application)

## Repository layout

```
Application-Framework.sln       Visual Studio solution
README.md                       This file
.github/workflows/build.yml     GitHub Actions pipeline (see Continuous integration)
Core/                           Static library (Core.lib): the framework
  Application.h/.cpp            Application lifecycle base class
  INI_Manager.h/.cpp            INI file configuration
  Logger.h/.cpp                 File logger
  LogParser.h/.cpp              Reads log files back
  CommandLineParser.h/.cpp      Command line flags and options
CoreTests/                      Google Test unit tests for Core
Samples/
  FeatureTour/                  Tour of every framework feature
  SingleInstance/               Allows only one running copy
  NetworkAdapters/              Lists network adapters (IP Helper API)
packages/                       NuGet packages (googletest); not committed, restored before building
```

## Building

Requirements:

- Visual Studio 2022 or later with the **Desktop development with C++** workload.
- The **MSVC v143** build tools: every project uses platform toolset v143. Visual Studio 2022
  includes them; with a newer Visual Studio, add the "MSVC v143 - VS 2022 C++ x64/x86 build
  tools" component in the Visual Studio Installer.

All projects use C++20. The solution targets **x64** in Debug and Release.

- **Visual Studio:** open `Application-Framework.sln` and build. Visual Studio restores googletest
  for `CoreTests` automatically on the first build.
- **Command line** (Developer Command Prompt): restore the NuGet packages first. Plain `msbuild`
  doesn't restore packages listed in `packages.config`, so a fresh clone fails to build
  `CoreTests` without this step.

  ```bash
  msbuild Application-Framework.sln /t:Restore /p:RestorePackagesConfig=true
  ```

  ```bash
  msbuild Application-Framework.sln /p:Configuration=Debug /p:Platform=x64 /m
  ```

All outputs go to `x64\<Configuration>\` at the solution root: `Core.lib`, `CoreTests.exe` and
one `.exe` per sample. Each sample depends on `Core` in the solution, so Core is always built
first.

Applications read their INI file and write their log file in the **current working directory**.
When you run an app with F5 in Visual Studio, that is the project folder by default; when you run
the `.exe` directly, it is wherever you start it from.

## The Core library

Everything is in the `Core::Framework` namespace, except `INI_Manager` which is in
`Core::Config`.

### Application

`Application` (`Core/Application.h`) is the base class for every app.

```cpp
// Public
explicit Application(const string& appName, const string& iniFile = "");
int run();
void setExitPrompt(bool enabled);

// Override in your class
virtual void execute() = 0;             // required: your application logic
virtual void initialize();              // optional: call Application::initialize() first
virtual void cleanup();                 // optional: call Application::cleanup() last
virtual void addDefaultConfig();        // optional: your defaults for a missing INI file
```

`run()` performs the whole lifecycle:

1. **`initialize()`**: loads `<appName>.ini` (or `iniFile` if given) and configures the logger
   to write to `<appName>.log` at the level set in `[Logging] log_level`.
   If the INI file doesn't exist it is created with defaults, and this is printed:

   ```
   Warning: Cannot open INI file: MyApp.ini, creating one with defaults
   ```

   The default file contains:

   ```ini
   [Logging]
   log_level = INFO
   ```

   Override **`addDefaultConfig()`** to add your application's own defaults to that file (and to
   change the default `log_level`). It is called only when the file is missing:

   ```cpp
   void addDefaultConfig() override
   {
       config.set("Logging", "log_level", "DEBUG");
       config.set("Greeter", "name", "World");
   }
   ```

   The log then records `INI file 'MyApp.ini' not found; created it with default settings
   (log_level = DEBUG).` When the file exists, it records
   `Loaded settings from 'MyApp.ini' (log_level = ...)` instead.

2. **`execute()`**: your application logic (pure virtual, must be implemented).
3. **`cleanup()`**: always runs, even when `initialize()` or `execute()` threw.
4. **Exit prompt**: prints the result, then waits for Enter:

   ```
   MyApp finished successfully (exit code 0).
   Press Enter to exit...
   ```

   On failure the first line reads `MyApp finished with errors (exit code 1).`
   The wait only happens in an interactive console, so scripted or piped runs never hang.
   Call `setExitPrompt(false)` to turn off both the line and the wait (the unit tests do this).

**Error handling:** no exception escapes `run()`. An exception from `initialize()`, `execute()`
or `cleanup()` is logged at `ERROR`, echoed to stderr as `Error: ...`, and makes `run()` return
**1**. Otherwise `run()` returns **0**. Return it from `main()` so scripts can check
`%ERRORLEVEL%`.

**Overriding:** when you override `initialize()` or `cleanup()`, call the base version.
Call `Application::initialize()` *first*, because it loads the configuration and opens the log;
call `Application::cleanup()` *last*.

**Available to derived classes:**

| Member | Description |
|---|---|
| `logger` | The `Logger` instance (static) |
| `config` | The `INI_Manager` instance, already loaded |
| `appName()` | The name passed to the constructor |

### INI_Manager

`INI_Manager` (`Core/INI_Manager.h`) holds the configuration. It is a singleton; inside an
`Application` use the `config` member.

```cpp
string get(const string& section, const string& key, const string& defaultValue = "") const;
void set(const string& section, const string& key, const string& value);
bool load(const string& filename);
bool save(const string& filename) const;
void clear();
vector<string> getSectionsMatching(const string& prefix) const;
vector<string> getAllSections() const;                              // in file order
map<string, map<string, string>> getAllData() const;                // section -> key -> value
map<string, map<string, string>> getAllDataWithGlobals() const;     // plus globals under "GLOBAL"
```

File format:

```ini
; Comment lines start with ';' or '#'
global_key = value

[Section]
key = value
```

- Keys before the first section are global: read them with `get("", "global_key")`.
- Whitespace around keys and values is trimmed. Comments must be on their own line; text after
  a value (such as `key = value ; note`) becomes part of the value.
- Files may be saved as UTF-8 with or without a byte order mark (BOM).
- `get()` returns `defaultValue` when the section or key doesn't exist. Values are always
  strings; convert them yourself (for example with `stoi`, and catch the exception for bad input).
- `load()` replaces the current data only if the file could be read. If it can't, it prints
  `Warning: Cannot open INI file: <name>`, returns `false` and keeps the previous data.
- `save()` writes globals first, then each section in order. Comments from a loaded file are not
  kept. If the file can't be written it prints `Error: ...` and returns `false`.
- `getSectionsMatching("Task.")` returns every section whose name **starts with** `Task.`, in
  file order. The prefix is literal text, not a regular expression; a single trailing `*` is
  accepted and ignored (`"Task.*"` is the same as `"Task."`).

### Logger

`Logger` (`Core/Logger.h`) writes timestamped lines to a file. It is a singleton; inside an
`Application` use the `logger` member, which `initialize()` has already configured.

```cpp
logger.log(LogLevel::INFO, "Something happened");
```

Output line format:

```
2026-10-11 16:44:05 [INFO] Something happened
```

Levels, lowest to highest: `STACK`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `NONE`.
A message is written when its level is at or above the configured `log_level`; `NONE` turns
logging off. Set the level in the INI file (names are case-insensitive):

```ini
[Logging]
log_level = DEBUG
```

An unknown name (a typo such as `log_level = verbose`) prints
`Warning: Unknown log_level 'verbose' ...` at startup and turns logging off. If the `[Logging]`
section or `log_level` key is missing from an existing INI file, logging is off too.

The log file is opened in append mode, so it grows across runs. If it can't be opened (for
example, the folder is read-only), `Warning: Cannot open log file: ...` is printed and log
messages are discarded. Logging goes only to the file, not to the console.

To use the logger outside an `Application` (in a tool or a test):

| Method | Description |
|---|---|
| `Logger::getInstance()` | The singleton |
| `setLogFile(path)` | Opens `path` in append mode, closing any previous file |
| `closeLogFile()` | Closes the file, so it can be deleted or moved |
| `setLogLevel(level)` | Sets the minimum level written |
| `loadConfig(path)` | Reads `log_level = <level>` from a plain `key = value` file |
| `Logger::tryParseLevel(name, level)` | Converts a level name; returns `false` for an unknown name |

### LogParser

`LogParser` (`Core/LogParser.h`) reads a log file written by `Logger` back into entries.

```cpp
LogParser parser("MyApp.log");
parser.parseLogs();                     // throws runtime_error if the file can't be opened

for (const auto& e : parser.filterBySeverity("WARN"))
{
    cout << e.m_sTimestamp << "  " << e.m_sMessage << endl;
}
```

| Method | Description |
|---|---|
| `parseLogs()` | Reads the file, replacing any earlier result; safe to call again |
| `getAllLogs()` | All entries (`m_sTimestamp`, `m_sSeverity`, `m_sMessage`) |
| `filterBySeverity(s)` | Entries whose severity equals `s` exactly (case-sensitive) |
| `getSkippedLineCount()` | Non-blank lines before the first valid entry |

Timestamps may include milliseconds (`HH:MM:SS.mmm`). A line that doesn't start a new entry
(a stack trace, a multi-line message) is appended to the previous entry's message.

### CommandLineParser

`CommandLineParser` (`Core/CommandLineParser.h`) handles single-dash flags and options.

```cpp
CommandLineParser cli;
cli.addFlag("h", "Show this help");
cli.addOption("name", "Name to greet", true);       // true = requires a value
cli.parse(argc, argv);

if (cli.hasFlag("h")) { cli.printHelp("MyApp"); }
string name = cli.getOptionValue("name");           // "" if not given
```

- Arguments are written with a single dash: `-h`, `-name Alice`.
- Register every flag and option **before** calling `parse()`. (The
  `CommandLineParser(argc, argv)` constructor parses immediately, before anything is registered,
  so prefer the default constructor.)
- Unknown options print `Warning: Unknown option -x` to stderr; a missing value prints an error
  to stderr. Neither throws.
- Calling `parse()` again replaces the previous result rather than adding to it.
- An argument starting with `-` is always treated as an option, so an option value can't start
  with `-`: `-count -5` prints `Error: Option -count requires a value.` and then
  `Warning: Unknown option -5`.

## Sample applications

The samples live under `Samples\` and appear in the solution's **Samples** folder. Each is a
single `.cpp` file with a class derived from `Application`.

### FeatureTour

`Samples/FeatureTour/FeatureTour.cpp`: a tour of the whole framework: lifecycle overrides,
INI configuration, command line options, logging, `LogParser`, and error handling.

What it does:

1. Greets a name, `repeat` times. The name comes from `-name`, else `[Greeter] name` in the INI,
   else `World`. `repeat` must be a whole number of at least 1; anything else is logged as a
   warning (with the bad value) and 1 is used.
2. Runs every `[Task.*]` section in the INI whose `enabled` is true, printing its `message`.
   `enabled` accepts `true/false`, `yes/no`, `on/off` or `1/0` in any case; any other value
   skips the task with a warning.
3. With `-report`, reads `FeatureTour.log` back with `LogParser` and prints a count per severity.

**What gets logged:** everything shown on screen (greetings, task messages, the `-report`
summary), plus the command line used, the help request, where the greeted name came from, a task
summary (`Tasks: 2 run, 1 skipped`), and at `DEBUG` level every setting in effect. An excerpt,
with timestamps removed:

```
[INFO] Loaded settings from 'FeatureTour.ini' (log_level = DEBUG).
[INFO] Command line: -name Alice -report
[DEBUG] Config [Greeter] repeat = 2
[DEBUG] Greeting 'Alice' (from command line (-name)) 2 time(s)
[INFO] Hello, Alice!
[INFO] [Task.Backup] Backing up files...
[INFO] Skipping disabled task Task.Cleanup
[INFO] Tasks: 2 run, 1 skipped
```

**Missing INI file:** FeatureTour overrides `addDefaultConfig()`, so if `FeatureTour.ini` is
missing (for example, the exe is run from another folder) it is recreated with the full settings
below, including `log_level = DEBUG`, rather than the framework's minimal `[Logging]` default.

Options:

| Option | Description |
|---|---|
| `-h` | Show help |
| `-name <value>` | Name to greet (overrides the INI) |
| `-report` | Print a severity summary of the log file at the end |
| `-fail` | Throw an exception, to show how `run()` handles errors (exit code 1) |

Configuration: `Samples/FeatureTour/FeatureTour.ini`, copied next to the `.exe` on every build
(the shipped file also has comments describing each setting):

```ini
[Logging]
log_level = DEBUG

[Greeter]
name = World
repeat = 2

[Task.Backup]
enabled = true
message = Backing up files...

[Task.Cleanup]
enabled = false
message = Cleaning temp files...

[Task.Report]
enabled = true
message = Generating report...
```

Example:

```
> FeatureTour.exe -name Alice -report
Hello, Alice!
Hello, Alice!
[Task.Backup] Backing up files...
[Task.Report] Generating report...

Log summary (22 entries):
  DEBUG: 12
  INFO: 10

FeatureTour finished successfully (exit code 0).
Press Enter to exit...
```

These counts are from a first run. The summary counts every entry in `FeatureTour.log`, which
grows across runs, so the numbers go up each time the app runs.

Edit the INI in the project folder, not the copy in `x64\<Configuration>\`; the build
overwrites the copy when the project's version is newer.

### SingleInstance

`Samples/SingleInstance/SingleInstance.cpp`: allows only one running copy at a time.

`initialize()` creates a named Windows mutex (`Core.Framework.Samples.SingleInstance`). If it
already exists, another copy is running: `initialize()` throws, so the framework reports the error
and the app exits with code 1. The mutex is released in the destructor, so it is still held while
the first copy waits at "Press Enter to exit...". Windows also removes it if the process crashes,
so a stale lock can never block startup.

To try it, start `SingleInstance.exe`, leave it at the prompt, and start a second copy:

```
> SingleInstance.exe
Error: Unhandled exception: Another instance of SingleInstance is already running

SingleInstance finished with errors (exit code 1).
Press Enter to exit...
```

The lock applies within one Windows login session. To allow only one copy across all users on
the machine, prefix the mutex name with `Global\`.

Note: the source defines `NOGDI` before including `<windows.h>`. Without it, `wingdi.h` defines a
macro named `ERROR` that breaks `LogLevel::ERROR`. Do the same in any app that includes Windows
headers.

### NetworkAdapters

`Samples/NetworkAdapters/NetworkAdapters.cpp`: lists the machine's network adapters using the
IP Helper API (`GetAdaptersInfo`, linked from `IPHLPAPI.lib`).

For each adapter it prints the name, description, MAC address, index, type (Ethernet, Wireless,
Loopback, ...), every IPv4 address and mask, every gateway, DHCP status with the lease times, and
WINS servers. Every line shown on screen is also written to `NetworkAdapters.log` at `INFO`
level, so the log keeps a full record of each run (with `log_level` set to `WARN` or higher, the
details appear on screen only). Part of an entry (addresses vary by machine):

```
	Adapter Desc: 	Intel(R) Wi-Fi 6E AX211 160MHz
	Adapter Addr: 	XX-XX-XX-XX-XX-XX
	Type: 	Wireless (802.11)
	IP Address: 	192.168.1.23
	IP Mask: 	255.255.255.0
	Gateway: 	192.168.1.1
	***
	DHCP Enabled: Yes
	  DHCP Server: 	192.168.1.1
	  Lease Obtained: 2026-10-11 10:03:27
	  Lease Expires:  2026-10-13 10:03:27
	Have Wins: No
```

Disconnected adapters show `(none)` for lease times. If `GetAdaptersInfo` fails, the error is
reported through the framework and the exit code is 1. `GetAdaptersInfo` reports IPv4 only;
`GetAdaptersAddresses` would be needed for IPv6.

## Tests

`CoreTests` is a Google Test project (NuGet package
`Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn`) covering the Core library:

| Test suite | Tests |
|---|---|
| `ApplicationTest` | 22 |
| `ApplicationLoggingTest` | 10 |
| `INI_ManagerTest` | 13 |
| `CommandLineParserTest` | 10 |
| `LogParserTest` | 8 |
| `LoggerTest` | 8 |

Run them with **Test Explorer** in Visual Studio, or run `x64\Debug\CoreTests.exe`. The
`Warning: Unknown option ...` lines printed during the run are expected; they come from tests that
pass unknown options on purpose.

`CoreTests` has its own `main()` (`CoreTests/main.cpp`) instead of the package's `gtest_main`,
so that it waits for Enter when run in a console window. It doesn't wait when output is
redirected, so Test Explorer and CI runs aren't affected.

## Continuous integration

`.github/workflows/build.yml` runs on GitHub Actions for pushes and pull requests to `main` or
`master`, and for tags matching `v*.*.*`.

| Job | What it does |
|---|---|
| **build** (Debug and Release, x64, `windows-2022`) | Restores NuGet packages, builds the solution, runs `CoreTests` (results uploaded as `TestResults-<Configuration>-x64`), and uploads a package as `<Configuration>-x64` |
| **release** (tags only) | After both builds pass, zips the Release package and attaches it to a GitHub Release with generated release notes |

The package contains:

```
bin\        FeatureTour.exe, FeatureTour.ini, SingleInstance.exe, NetworkAdapters.exe
lib\        Core.lib
include\    Core headers
README.md
```

To publish a release, push a version tag:

```bash
git tag v1.0.0
```

```bash
git push origin v1.0.0
```

## Creating a new application

1. In Visual Studio, add a new **Console App** project under `Samples\` (or anywhere) and add it
   to the solution.
2. Point it at Core, in the project properties for each x64 configuration:
   - **C/C++ > General > Additional Include Directories:** the `Core` folder
     (`..\..\Core` from `Samples\<Name>\`)
   - **C/C++ > Language > C++ Language Standard:** ISO C++20 (`/std:c++20`), the same as Core
   - **Linker > General > Additional Library Directories:** `$(SolutionDir)$(Platform)\$(Configuration)\`
   - **Linker > Input > Additional Dependencies:** `Core.lib`
3. Right-click the project, choose **Build Dependencies > Project Dependencies**, and tick `Core`.
4. Derive a class from `Application`, pass the app name to its constructor, implement
   `execute()`, and return `app.run()` from `main()` (see the example at the top). If you
   include `<windows.h>`, define `NOGDI` first (see [SingleInstance](#singleinstance)).

Copying one of the sample projects and renaming it is the quickest route; all three are already
set up this way. A copied project needs a new `ProjectGuid` in its `.vcxproj` before it is added
to the solution.
