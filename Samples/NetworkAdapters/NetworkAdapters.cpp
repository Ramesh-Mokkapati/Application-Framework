// NetworkAdapters.cpp : Lists the machine's network adapters (IPv4) using GetAdaptersInfo,
// built on the Core framework.
//
// For each adapter: name, description, MAC address, type, every IP address/mask and gateway,
// DHCP lease details and WINS servers. Every line shown on screen is also written to
// NetworkAdapters.log at INFO level (see show()). A GetAdaptersInfo failure is thrown, so
// Application::run() logs it, prints it to stderr and returns exit code 1.

// wingdi.h defines ERROR as a macro, which would break LogLevel::ERROR
#define NOGDI
#define NOMINMAX
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")

#include <vector>

#include "Application.h"

using namespace Core::Framework;

class NetworkAdaptersApp : public Application
{
public:
    NetworkAdaptersApp()
        : Application("NetworkAdapters")          // reads NetworkAdapters.ini, logs to NetworkAdapters.log
    {
    }

protected:
    void initialize() override
    {
        // Loads the INI and configures the logger
        Application::initialize();

        logger.log(LogLevel::INFO, "NetworkAdapters initialized");
    }

    void execute() override
    {
        logger.log(LogLevel::INFO, "NetworkAdapters running");

        // Must stay in this vector (no copies): the adapters' Next pointers point into it
        const vector<BYTE> buffer = getAdaptersInfo();
        if (buffer.empty())
        {
            show("No network adapters found.");
            return;
        }

        int count = 0;
        for (auto adapter = reinterpret_cast<const IP_ADAPTER_INFO*>(buffer.data()); adapter != nullptr; adapter = adapter->Next)
        {
            printAdapter(*adapter);
            ++count;
        }

        logger.log(LogLevel::INFO, "Listed " + to_string(count) + " adapter(s)");
    }

    void cleanup() override
    {
        logger.log(LogLevel::INFO, "NetworkAdapters shutting down");
        Application::cleanup();
    }

private:
    // Returns the buffer GetAdaptersInfo filled (a linked list of IP_ADAPTER_INFO),
    // or an empty buffer if the machine has no adapters. Throws on failure.
    static vector<BYTE> getAdaptersInfo()
    {
        ULONG size = sizeof(IP_ADAPTER_INFO);
        vector<BYTE> buffer;

        // ERROR_BUFFER_OVERFLOW updates size; retry because an adapter can be added between calls
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            buffer.resize(size);

            const DWORD result = GetAdaptersInfo(reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data()), &size);
            if (result == NO_ERROR)
            {
                return buffer;
            }

            if (result == ERROR_NO_DATA)
            {
                return {};
            }

            if (result != ERROR_BUFFER_OVERFLOW)
            {
                throw runtime_error("GetAdaptersInfo failed with error " + to_string(result));
            }
        }

        throw runtime_error("GetAdaptersInfo failed: the adapter list kept changing size");
    }

    // Displays a line on screen and writes the same text to the log (INFO level)
    static void show(const string& line)
    {
        cout << line << endl;
        logger.log(LogLevel::INFO, line);
    }

    static void printAdapter(const IP_ADAPTER_INFO& adapter)
    {
        show("\tComboIndex: \t" + to_string(adapter.ComboIndex));
        show(string("\tAdapter Name: \t") + adapter.AdapterName);
        show(string("\tAdapter Desc: \t") + adapter.Description);
        show("\tAdapter Addr: \t" + formatMacAddress(adapter.Address, adapter.AddressLength));
        show("\tIndex: \t" + to_string(adapter.Index));
        show("\tType: \t" + adapterTypeName(adapter.Type));

        // An adapter can have several IP addresses and gateways
        for (auto ip = &adapter.IpAddressList; ip != nullptr; ip = ip->Next)
        {
            show(string("\tIP Address: \t") + ip->IpAddress.String);
            show(string("\tIP Mask: \t") + ip->IpMask.String);
        }

        for (auto gateway = &adapter.GatewayList; gateway != nullptr; gateway = gateway->Next)
        {
            show(string("\tGateway: \t") + gateway->IpAddress.String);
        }

        show("\t***");

        if (adapter.DhcpEnabled)
        {
            show("\tDHCP Enabled: Yes");
            show(string("\t  DHCP Server: \t") + adapter.DhcpServer.IpAddress.String);
            show("\t  Lease Obtained: " + formatTime(adapter.LeaseObtained));
            show("\t  Lease Expires:  " + formatTime(adapter.LeaseExpires));
        }
        else
        {
            show("\tDHCP Enabled: No");
        }

        if (adapter.HaveWins)
        {
            show("\tHave Wins: Yes");
            show(string("\t  Primary Wins Server:    ") + adapter.PrimaryWinsServer.IpAddress.String);
            show(string("\t  Secondary Wins Server:  ") + adapter.SecondaryWinsServer.IpAddress.String);
        }
        else
        {
            show("\tHave Wins: No");
        }

        // Separator between adapters: screen only, an empty log entry would add nothing
        cout << endl;
    }

    // "00-1A-2B-3C-4D-5E"
    static string formatMacAddress(const BYTE* address, UINT length)
    {
        ostringstream mac;
        mac << hex << uppercase << setfill('0');

        for (UINT i = 0; i < length; ++i)
        {
            if (i > 0)
            {
                mac << '-';
            }

            mac << setw(2) << static_cast<int>(address[i]);
        }

        return mac.str();
    }

    static string adapterTypeName(UINT type)
    {
        switch (type)
        {
        case MIB_IF_TYPE_OTHER:     return "Other";
        case MIB_IF_TYPE_ETHERNET:  return "Ethernet";
        case MIB_IF_TYPE_TOKENRING: return "Token Ring";
        case MIB_IF_TYPE_FDDI:      return "FDDI";
        case MIB_IF_TYPE_PPP:       return "PPP";
        case MIB_IF_TYPE_LOOPBACK:  return "Loopback";
        case MIB_IF_TYPE_SLIP:      return "Slip";
        case IF_TYPE_IEEE80211:     return "Wireless (802.11)";
        default:                    return "Unknown type " + to_string(type);
        }
    }

    // Local time in the same format the Logger uses
    static string formatTime(time_t time)
    {
        // Disconnected adapters have no lease: the time is 0, which would print as 1970
        if (time == 0)
        {
            return "(none)";
        }

        tm local;
        if (localtime_s(&local, &time) != 0)
        {
            return "(invalid time)";
        }

        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
        return buffer;
    }
};

int main()
{
    NetworkAdaptersApp app;

    // initialize() -> execute() -> cleanup(); exceptions are handled inside.
    // Returns 0 on success, 1 on failure, so scripts can check the exit code.
    return app.run();
}
