// SingleInstance.cpp : Single-instance application built on the Core framework.
//
// Only one copy can run at a time (per Windows session). A second copy fails in initialize(),
// which Application::run() reports as an error (logged + stderr) and turns into exit code 1.

// wingdi.h defines ERROR as a macro, which would break LogLevel::ERROR
#define NOGDI
#define NOMINMAX
#include <windows.h>

#include "Application.h"

using namespace Core::Framework;

class SingleInstanceApp : public Application
{
public:
    SingleInstanceApp()
        : Application("SingleInstance")          // reads SingleInstance.ini, logs to SingleInstance.log
    {
    }

    // The lock is held until here (not released in cleanup()), so it is still held while
    // run() waits at "Press Enter to exit..." and a second copy can be started to test it
    ~SingleInstanceApp() override
    {
        releaseInstanceLock();
    }

protected:
    void initialize() override
    {
        // Loads the INI and configures the logger, so the lock result can be logged
        Application::initialize();

        acquireInstanceLock();

        logger.log(LogLevel::INFO, "SingleInstance initialized");
    }

    void execute() override
    {
        logger.log(LogLevel::INFO, "SingleInstance running");

        cout << "SingleInstance is running. Start another copy before exiting to see it rejected." << endl;
    }

    void cleanup() override
    {
        logger.log(LogLevel::INFO, "SingleInstance shutting down");
        Application::cleanup();
    }

private:
    // Named mutex used as an "already running" marker. Windows destroys it when the last
    // handle is closed, including when the process crashes, so a stale lock can't block startup.
    // The name must be unique to this application; it is scoped to the current session.
    static constexpr const wchar_t* kInstanceMutexName = L"Core.Framework.Samples.SingleInstance";

    void acquireInstanceLock()
    {
        // No initial ownership needed: only the mutex's existence matters
        m_hInstanceMutex = CreateMutexW(nullptr, FALSE, kInstanceMutexName);
        const DWORD lastError = GetLastError();

        if (m_hInstanceMutex == nullptr)
        {
            throw runtime_error("Failed to create single-instance mutex (error " + to_string(lastError) + ")");
        }

        if (lastError == ERROR_ALREADY_EXISTS)
        {
            // This handle refers to the other instance's mutex: close it, don't release it
            releaseInstanceLock();
            throw runtime_error("Another instance of SingleInstance is already running");
        }

        logger.log(LogLevel::DEBUG, "Single-instance lock acquired");
    }

    void releaseInstanceLock()
    {
        if (m_hInstanceMutex != nullptr)
        {
            CloseHandle(m_hInstanceMutex);
            m_hInstanceMutex = nullptr;
        }
    }

    HANDLE      m_hInstanceMutex = nullptr;
};

int main()
{
    SingleInstanceApp app;

    // initialize() -> execute() -> cleanup(); exceptions are handled inside.
    // Returns 0 on success, 1 on failure (including "already running").
    return app.run();
}
