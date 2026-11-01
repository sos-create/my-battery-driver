#include <windows.h>
#include <winioctl.h>
#ifndef FILE_DEVICE_BATTERY
#define FILE_DEVICE_BATTERY 0x00000029
#endif
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <powrprof.h>
#include <poclass.h>
#include "BatteryAdapters.h"
#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(lib, "PowrProf.lib")

#define ID_REFRESH 1001
#define ID_SAVER 1002
#define ID_BALANCED 1003
#define ID_PERFORMANCE 1004
#define ID_OUTPUT 1100

static HWND g_output;

static const GUID GUID_UBM_BALANCED =
{ 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5f, 0xf5, 0x5b, 0x26, 0x0e } };
static const GUID GUID_UBM_PERFORMANCE =
{ 0x8c5e7fda, 0xe8bf, 0x4a96, { 0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c } };
static const GUID GUID_UBM_SAVER =
{ 0xa1841308, 0x3541, 0x4fab, { 0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a } };

static void AddText(const WCHAR* format, ...)
{
    WCHAR buffer[2048];
    va_list args;
    int length;

    va_start(args, format);
    length = _vsnwprintf_s(buffer, (sizeof(buffer) / sizeof(buffer[0])), _TRUNCATE, format, args);
    va_end(args);

    if (length < 0)
        buffer[(sizeof(buffer) / sizeof(buffer[0])) - 1] = L'\0';

    if (g_output != NULL)
    {
        SendMessageW(g_output, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
        SendMessageW(g_output, EM_REPLACESEL, FALSE, (LPARAM)buffer);
    }
}

static void ShowActivePowerPlan(void)
{
    GUID* active = NULL;
    WCHAR name[256];
    DWORD size = sizeof(name);
    DWORD result = PowerGetActiveScheme(NULL, &active);

    if (result != ERROR_SUCCESS || active == NULL)
    {
        AddText(L"Windows power plan: unavailable (error %lu)\r\n", result);
        return;
    }

    ZeroMemory(name, sizeof(name));
    result = PowerReadFriendlyName(
        NULL, active, NULL, NULL, (UCHAR*)name, &size
    );

    AddText(L"Active Windows power plan: %ls\r\n",
        result == ERROR_SUCCESS && name[0] ? name : L"Name unavailable");
    LocalFree(active);
}

static void ScanBatteryInterfaces(void)
{
    HDEVINFO set;
    DWORD index;
    DWORD count = 0;

    AddText(L"\r\n=== BATTERY INTERFACES ===\r\n");
    set = SetupDiGetClassDevsW(
        &GUID_DEVCLASS_BATTERY, NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (set == INVALID_HANDLE_VALUE)
    {
        AddText(L"Battery interface scan failed: %lu\r\n", GetLastError());
        return;
    }

    for (index = 0; index < 64; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData;
        DWORD required = 0;
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail;
        HANDLE battery;
        ULONG waitMs = 0;
        ULONG tag = BATTERY_TAG_INVALID;
        DWORD returned = 0;
        BATTERY_INFORMATION info;
        BATTERY_WAIT_STATUS waitStatus;
        BATTERY_STATUS status;
        BATTERY_QUERY_INFORMATION query;
        ULONG temperature = 0;
        BOOL hasTemperature = FALSE;

        ZeroMemory(&interfaceData, sizeof(interfaceData));
        interfaceData.cbSize = sizeof(interfaceData);

        if (!SetupDiEnumDeviceInterfaces(
            set, NULL, &GUID_DEVCLASS_BATTERY, index, &interfaceData))
        {
            break;
        }

        SetupDiGetDeviceInterfaceDetailW(
            set, &interfaceData, NULL, 0, &required, NULL
        );
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0)
            continue;

        detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, required);
        if (detail == NULL)
            continue;

        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(
            set, &interfaceData, detail, required, NULL, NULL))
        {
            HeapFree(GetProcessHeap(), 0, detail);
            continue;
        }

        battery = CreateFileW(
            detail->DevicePath, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
        );
        HeapFree(GetProcessHeap(), 0, detail);

        if (battery == INVALID_HANDLE_VALUE)
        {
            AddText(L"Battery interface %lu: open failed (%lu)\r\n",
                index + 1, GetLastError());
            continue;
        }

        if (!DeviceIoControl(
            battery, IOCTL_BATTERY_QUERY_TAG,
            &waitMs, sizeof(waitMs), &tag, sizeof(tag), &returned, NULL) ||
            tag == BATTERY_TAG_INVALID)
        {
            AddText(L"Battery interface %lu: no valid battery tag\r\n", index + 1);
            CloseHandle(battery);
            continue;
        }

        ZeroMemory(&info, sizeof(info));
        ZeroMemory(&query, sizeof(query));
        query.BatteryTag = tag;
        query.InformationLevel = BatteryInformation;
        if (!DeviceIoControl(
            battery, IOCTL_BATTERY_QUERY_INFORMATION,
            &query, sizeof(query), &info, sizeof(info), &returned, NULL))
        {
            AddText(L"Battery %lu: information query failed (%lu)\r\n",
                index + 1, GetLastError());
            CloseHandle(battery);
            continue;
        }

        ZeroMemory(&waitStatus, sizeof(waitStatus));
        waitStatus.BatteryTag = tag;
        ZeroMemory(&status, sizeof(status));
        if (!DeviceIoControl(
            battery, IOCTL_BATTERY_QUERY_STATUS,
            &waitStatus, sizeof(waitStatus),
            &status, sizeof(status), &returned, NULL))
        {
            AddText(L"Battery %lu: status query failed (%lu)\r\n",
                index + 1, GetLastError());
            CloseHandle(battery);
            continue;
        }

        query.InformationLevel = BatteryTemperature;
        hasTemperature = DeviceIoControl(
            battery, IOCTL_BATTERY_QUERY_INFORMATION,
            &query, sizeof(query), &temperature, sizeof(temperature),
            &returned, NULL);

        AddText(L"\r\nBattery #%lu\r\n", ++count);
        if (info.FullChargedCapacity != 0 &&
            info.FullChargedCapacity != BATTERY_UNKNOWN_CAPACITY &&
            status.Capacity != BATTERY_UNKNOWN_CAPACITY)
        {
            double percent = 100.0 * (double)status.Capacity /
                (double)info.FullChargedCapacity;
            if (percent > 100.0) percent = 100.0;
            AddText(L"  Charge: %.1f%%\r\n", percent);
        }
        else
            AddText(L"  Charge: not reported by device\r\n");

        if (status.Voltage != BATTERY_UNKNOWN_VOLTAGE)
            AddText(L"  Voltage: %lu mV\r\n", status.Voltage);
        else
            AddText(L"  Voltage: not reported\r\n");

        AddText(L"  State:");
        if (status.PowerState & BATTERY_CHARGING) AddText(L" charging");
        if (status.PowerState & BATTERY_DISCHARGING) AddText(L" discharging");
        if (status.PowerState & BATTERY_POWER_ON_LINE) AddText(L" AC-connected");
        if (status.PowerState & BATTERY_CRITICAL) AddText(L" critical");
        if (status.PowerState == 0) AddText(L" idle/unknown");
        AddText(L"\r\n");

        if (hasTemperature && temperature > 0 && temperature < 10000)
            AddText(L"  Temperature: %.1f C\r\n",
                ((double)temperature / 10.0) - 273.15);
        else
            AddText(L"  Temperature: not exposed by this battery\r\n");

        if (info.CycleCount)
            AddText(L"  Cycle count: %lu\r\n", info.CycleCount);
        else
            AddText(L"  Cycle count: not reported\r\n");

        if (info.DesignedCapacity != BATTERY_UNKNOWN_CAPACITY)
            AddText(L"  Design capacity: %lu %ls\r\n",
                info.DesignedCapacity,
                (info.Capabilities & BATTERY_CAPACITY_RELATIVE) ? L"relative units" : L"mWh");
        if (info.FullChargedCapacity != BATTERY_UNKNOWN_CAPACITY)
            AddText(L"  Full capacity: %lu %ls\r\n",
                info.FullChargedCapacity,
                (info.Capabilities & BATTERY_CAPACITY_RELATIVE) ? L"relative units" : L"mWh");

        CloseHandle(battery);
    }

    SetupDiDestroyDeviceInfoList(set);
    if (count == 0)
        AddText(L"No readable battery interface returned individual data.\r\n");
}

static void ScanBatteryPnP(void)
{
    HDEVINFO set;
    DWORD index;
    DWORD count = 0;

    AddText(L"\r\n=== BATTERY PNP DEVICES ===\r\n");
    set = SetupDiGetClassDevsW(
        &GUID_DEVCLASS_BATTERY, NULL, NULL, DIGCF_PRESENT
    );
    if (set == INVALID_HANDLE_VALUE)
    {
        AddText(L"PnP scan failed: %lu\r\n", GetLastError());
        return;
    }

    for (index = 0; index < 128; ++index)
    {
        SP_DEVINFO_DATA data;
        WCHAR name[512];
        WCHAR service[256];
        DWORD type = 0;
        DWORD needed = 0;
        ULONG status = 0, problem = 0;

        ZeroMemory(&data, sizeof(data));
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInfo(set, index, &data))
            break;

        ZeroMemory(name, sizeof(name));
        if (!SetupDiGetDeviceRegistryPropertyW(
            set, &data, SPDRP_FRIENDLYNAME, &type,
            (PBYTE)name, sizeof(name), &needed) &&
            !SetupDiGetDeviceRegistryPropertyW(
                set, &data, SPDRP_DEVICEDESC, &type,
                (PBYTE)name, sizeof(name), &needed))
        {
            wcscpy_s(name, (sizeof(name) / sizeof(name[0])), L"Unnamed battery device");
        }

        ZeroMemory(service, sizeof(service));
        SetupDiGetDeviceRegistryPropertyW(
            set, &data, SPDRP_SERVICE, &type,
            (PBYTE)service, sizeof(service), &needed
        );

        CM_Get_DevNode_Status(&status, &problem, data.DevInst, 0);
        AddText(L"%lu. %ls\r\n   Service: %ls | PnP problem: %lu\r\n",
            ++count, name, service[0] ? service : L"not reported", problem);
    }

    SetupDiDestroyDeviceInfoList(set);
    if (count == 0)
        AddText(L"No present battery-class PnP devices found.\r\n");
}

static void RefreshDashboard(void)
{
    SYSTEM_POWER_STATUS power;
    SetWindowTextW(g_output, L"");
    AddText(L"UNIVERSAL BATTERY MANAGER\r\n");
    AddText(L"Read-only battery diagnostics | Windows native APIs\r\n");
    AddText(L"Hardware register writes: disabled\r\n\r\n");

    if (GetSystemPowerStatus(&power))
    {
        if (power.ACLineStatus == 1) AddText(L"AC adapter: connected\r\n");
        else if (power.ACLineStatus == 0) AddText(L"AC adapter: disconnected\r\n");
        else AddText(L"AC adapter: unknown\r\n");

        if (power.BatteryLifePercent == 255)
            AddText(L"Windows aggregate charge: unavailable\r\n");
        else
            AddText(L"Windows aggregate charge: %u%%\r\n",
                (unsigned)power.BatteryLifePercent);

        if (power.BatteryFlag & 8) AddText(L"Windows status: charging\r\n");
        else if (power.BatteryFlag & 128) AddText(L"Windows status: no system battery\r\n");
        else AddText(L"Windows status: not charging / unknown\r\n");

        if (power.BatteryLifeTime != (DWORD)-1)
            AddText(L"Estimated remaining time: %lu minutes\r\n",
                power.BatteryLifeTime / 60);
        else
            AddText(L"Estimated remaining time: unavailable\r\n");
    }
    else
        AddText(L"GetSystemPowerStatus failed: %lu\r\n", GetLastError());

    UBM_LoadExternalAdapters();
    ShowActivePowerPlan();
    { size_t n=0,i; const UBM_ADAPTER_DESCRIPTOR* a=UBM_GetAdapterCatalog(&n); AddText(L"\\r\\n=== ADAPTER CATALOG ===\\r\\n"); for(i=0;i<n;++i) AddText(L"%ls: %ls\\r\\n",a[i].name,a[i].currentSupport); }
    AddText(L"External adapter modules loaded: %llu\\r\\n", (unsigned long long)UBM_GetExternalAdapterCount());
    { size_t ai; for (ai=0; ai<UBM_GetExternalAdapterCount(); ++ai) AddText(L"  Plugin: %ls\\r\\n", UBM_GetExternalAdapterName(ai)); }
    ScanBatteryPnP();
    ScanBatteryInterfaces();

    AddText(L"\r\n=== PROTOCOL AND SAFETY NOTES ===\r\n");
    AddText(L"Individual telemetry is queried through the Windows Battery Class interface.\r\n");
    AddText(L"PnP/ACPI/USB/I2C/SMBus identification does not prove a vendor protocol is supported.\r\n");
    AddText(L"Unknown controllers require a verified adapter/protocol before any write operation.\r\n");
}

static void ChangePlan(const GUID* plan)
{
    DWORD result = PowerSetActiveScheme(NULL, plan);
    if (result == ERROR_SUCCESS)
        RefreshDashboard();
    else
        MessageBoxW(NULL,
            L"Windows could not change the selected power plan.",
            L"Universal Battery Manager", MB_OK | MB_ICONWARNING);
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateWindowW(L"BUTTON", L"Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, 12, 100, 30, window, (HMENU)ID_REFRESH, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Power saver",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            120, 12, 110, 30, window, (HMENU)ID_SAVER, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Balanced",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            238, 12, 110, 30, window, (HMENU)ID_BALANCED, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Performance",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            356, 12, 120, 30, window, (HMENU)ID_PERFORMANCE, NULL, NULL);
        g_output = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_READONLY | WS_VSCROLL | WS_HSCROLL,
            12, 52, 900, 560, window, (HMENU)ID_OUTPUT, NULL, NULL
        );
        SendMessageW(g_output, WM_SETFONT,
            (WPARAM)GetStockObject(ANSI_FIXED_FONT), TRUE);
        SetTimer(window, 1, 5000, NULL);
        RefreshDashboard();
        return 0;

    case WM_SIZE:
        if (g_output != NULL)
            MoveWindow(g_output, 12, 52,
                LOWORD(lParam) - 24, HIWORD(lParam) - 64, TRUE);
        return 0;

    case WM_TIMER:
        if (wParam == 1) RefreshDashboard();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_REFRESH: RefreshDashboard(); return 0;
        case ID_SAVER: ChangePlan(&GUID_UBM_SAVER); return 0;
        case ID_BALANCED: ChangePlan(&GUID_UBM_BALANCED); return 0;
        case ID_PERFORMANCE: ChangePlan(&GUID_UBM_PERFORMANCE); return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR commandLine, int show)
{
    const WCHAR className[] = L"UniversalBatteryManagerWindow";
    WNDCLASSW wc;
    HWND window;
    MSG message;

    (void)previous;
    if (commandLine != NULL && wcsstr(commandLine, L"--smoke-test") != NULL)
    {
        SYSTEM_POWER_STATUS power;
        HDEVINFO batterySet;
        if (!GetSystemPowerStatus(&power)) return 2;
        batterySet = SetupDiGetClassDevsW(
            &GUID_DEVCLASS_BATTERY, NULL, NULL, DIGCF_PRESENT
        );
        if (batterySet == INVALID_HANDLE_VALUE) return 3;
        SetupDiDestroyDeviceInfoList(batterySet);
        return 0;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc))
        return 1;

    window = CreateWindowExW(
        0, className, L"Universal Battery Manager",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 680,
        NULL, NULL, instance, NULL
    );
    if (window == NULL)
        return 1;

    ShowWindow(window, show);
    UpdateWindow(window);

    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
