#include <initguid.h>
#include "BatteryDriver.h"

static void PrintSeparator(void)
{
    wprintf(
        L"------------------------------------------------------------\n"
    );
}

BOOL ContainsTextInsensitive(
    const WCHAR* text,
    const WCHAR* part
)
{
    size_t length;

    if (text == NULL || part == NULL)
        return FALSE;

    length = wcslen(part);

    if (length == 0)
        return TRUE;

    while (*text != L'\0')
    {
        size_t i = 0;

        while (
            i < length &&
            text[i] != L'\0' &&
            towlower(text[i]) == towlower(part[i])
        )
        {
            i++;
        }

        if (i == length)
            return TRUE;

        text++;
    }

    return FALSE;
}

static void ReadRegistryString(
    HDEVINFO deviceSet,
    SP_DEVINFO_DATA* deviceData,
    DWORD property,
    WCHAR* buffer,
    DWORD bufferChars
)
{
    DWORD dataType = 0;
    DWORD required = 0;

    if (buffer == NULL || bufferChars == 0)
        return;

    ZeroMemory(buffer, bufferChars * sizeof(WCHAR));

    SetupDiGetDeviceRegistryPropertyW(
        deviceSet,
        deviceData,
        property,
        &dataType,
        (PBYTE)buffer,
        bufferChars * sizeof(WCHAR),
        &required
    );

    buffer[bufferChars - 1] = L'\0';
}

static void ReadHardwareIds(
    HDEVINFO deviceSet,
    SP_DEVINFO_DATA* deviceData,
    WCHAR* buffer,
    DWORD bufferChars
)
{
    DWORD dataType = 0;
    DWORD required = 0;
    DWORD i;

    if (buffer == NULL || bufferChars == 0)
        return;

    ZeroMemory(buffer, bufferChars * sizeof(WCHAR));

    if (!SetupDiGetDeviceRegistryPropertyW(
        deviceSet,
        deviceData,
        SPDRP_HARDWAREID,
        &dataType,
        (PBYTE)buffer,
        bufferChars * sizeof(WCHAR),
        &required
    ))
    {
        buffer[0] = L'\0';
        return;
    }

    for (i = 0; i + 1 < bufferChars; i++)
    {
        if (buffer[i] == L'\0')
        {
            if (buffer[i + 1] == L'\0')
                break;

            buffer[i] = L';';
        }
    }

    buffer[bufferChars - 1] = L'\0';
}

BOOL ReadDeviceInfo(
    HDEVINFO deviceSet,
    SP_DEVINFO_DATA* deviceData,
    DEVICE_INFO* output
)
{
    CONFIGRET result;
    ULONG problem = 0;
    ULONG status = 0;

    if (
        deviceSet == INVALID_HANDLE_VALUE ||
        deviceData == NULL ||
        output == NULL
    )
    {
        return FALSE;
    }

    ZeroMemory(output, sizeof(DEVICE_INFO));

    if (!SetupDiGetDeviceInstanceIdW(
        deviceSet,
        deviceData,
        output->instanceId,
        MAX_INSTANCE_ID,
        NULL
    ))
    {
        return FALSE;
    }

    ReadRegistryString(
        deviceSet, deviceData, SPDRP_FRIENDLYNAME,
        output->friendlyName, MAX_DEVICE_NAME
    );

    if (output->friendlyName[0] == L'\0')
    {
        ReadRegistryString(
            deviceSet, deviceData, SPDRP_DEVICEDESC,
            output->friendlyName, MAX_DEVICE_NAME
        );
    }

    ReadRegistryString(
        deviceSet, deviceData, SPDRP_DEVICEDESC,
        output->description, MAX_DEVICE_NAME
    );

    ReadRegistryString(
        deviceSet, deviceData, SPDRP_CLASS,
        output->className, MAX_CLASS_NAME
    );

    ReadRegistryString(
        deviceSet, deviceData, SPDRP_MFG,
        output->manufacturer, MAX_MANUFACTURER
    );

    ReadHardwareIds(
        deviceSet, deviceData,
        output->hardwareIds, MAX_HARDWARE_IDS
    );

    ReadRegistryString(
        deviceSet, deviceData, SPDRP_SERVICE,
        output->serviceName, MAX_SERVICE_NAME
    );

    output->isPresent = TRUE;

    result = CM_Get_DevNode_Status(
        &status, &problem, deviceData->DevInst, 0
    );

    if (result == CR_SUCCESS)
    {
        DEVPROP_BOOLEAN present = DEVPROP_FALSE;
        DEVPROPTYPE propertyType = DEVPROP_TYPE_EMPTY;

        output->devNodeStatus = status;
        output->problemCode = problem;
        output->hasProblem = (problem != 0);

        if (SetupDiGetDevicePropertyW(
            deviceSet,
            deviceData,
            &DEVPKEY_Device_IsPresent,
            &propertyType,
            (PBYTE)&present,
            sizeof(present),
            NULL,
            0
        ))
        {
            if (propertyType == DEVPROP_TYPE_BOOLEAN)
                output->isPresent = (present == DEVPROP_TRUE);
        }
    }

    output->isBattery =
        ContainsTextInsensitive(output->className, L"Battery") ||
        ContainsTextInsensitive(output->instanceId, L"PNP0C0A") ||
        ContainsTextInsensitive(output->friendlyName, L"battery") ||
        ContainsTextInsensitive(output->friendlyName, L"батар");

    output->isAcpiDevice =
        ContainsTextInsensitive(output->instanceId, L"ACPI\\");

    output->isEmbeddedController =
        ContainsTextInsensitive(output->instanceId, L"PNP0C09") ||
        ContainsTextInsensitive(output->friendlyName, L"Embedded Controller") ||
        ContainsTextInsensitive(output->description, L"Embedded Controller");

    output->isSmbusDevice =
        ContainsTextInsensitive(output->friendlyName, L"SMBus") ||
        ContainsTextInsensitive(output->description, L"SMBus") ||
        ContainsTextInsensitive(output->hardwareIds, L"SMBUS");

    output->isI2cDevice =
        ContainsTextInsensitive(output->friendlyName, L"I2C") ||
        ContainsTextInsensitive(output->description, L"I2C") ||
        ContainsTextInsensitive(output->hardwareIds, L"I2C");

    output->isUsbDevice =
        ContainsTextInsensitive(output->instanceId, L"USB\\");

    output->isBusController =
        output->isSmbusDevice || output->isI2cDevice;

    output->isPowerController =
        ContainsTextInsensitive(output->instanceId, L"PNP0C09") ||
        ContainsTextInsensitive(output->instanceId, L"PNP0C0C") ||
        output->isEmbeddedController ||
        output->isBusController ||
        ContainsTextInsensitive(output->friendlyName, L"Power Management");

    return TRUE;
}

BOOL IsRelevantDevice(const DEVICE_INFO* device)
{
    if (device == NULL)
        return FALSE;

    return device->isBattery ||
        device->isAcpiDevice ||
        device->isPowerController ||
        device->isBusController ||
        (device->isUsbDevice && device->hasProblem) ||
        device->hasProblem;
}

void PrintDevice(const DEVICE_INFO* device)
{
    if (device == NULL)
        return;

    wprintf(L"Name             : %ls\n",
        device->friendlyName[0] ? device->friendlyName : L"Unknown");
    wprintf(L"Class            : %ls\n",
        device->className[0] ? device->className : L"Unknown");
    wprintf(L"Manufacturer     : %ls\n",
        device->manufacturer[0] ? device->manufacturer : L"Unknown");
    wprintf(L"Instance ID      : %ls\n", device->instanceId);
    wprintf(L"Hardware IDs     : %ls\n",
        device->hardwareIds[0] ? device->hardwareIds : L"Unknown");
    wprintf(L"Present          : %ls\n",
        device->isPresent ? L"YES" : L"NO");
    wprintf(L"Battery          : %ls\n",
        device->isBattery ? L"YES" : L"NO");
    wprintf(L"ACPI             : %ls\n",
        device->isAcpiDevice ? L"YES" : L"NO");
    wprintf(L"Embedded Control : %ls\n",
        device->isEmbeddedController ? L"YES" : L"NO");
    wprintf(L"SMBus / I2C      : %ls\n",
        device->isBusController ? L"YES" : L"NO");

    if (device->hasProblem)
        wprintf(L"PnP ERROR        : %lu\n", device->problemCode);
    else
        wprintf(L"PnP ERROR        : NONE\n");
}

void PrintBatteryStatus(void)
{
    SYSTEM_POWER_STATUS power;

    wprintf(L"\n=== WINDOWS AGGREGATE POWER STATUS ===\n");
    ZeroMemory(&power, sizeof(power));

    if (!GetSystemPowerStatus(&power))
    {
        wprintf(L"Cannot obtain power status. Error: %lu\n", GetLastError());
        return;
    }

    if (power.ACLineStatus == 1)
        wprintf(L"AC adapter       : CONNECTED\n");
    else if (power.ACLineStatus == 0)
        wprintf(L"AC adapter       : DISCONNECTED\n");
    else
        wprintf(L"AC adapter       : UNKNOWN\n");

    if (power.BatteryLifePercent == 255)
        wprintf(L"Battery charge   : UNKNOWN\n");
    else
        wprintf(L"Battery charge   : %u%%\n",
            (unsigned)power.BatteryLifePercent);

    if (power.BatteryFlag == 255)
        wprintf(L"Battery status   : UNKNOWN\n");
    else if (power.BatteryFlag & 128)
        wprintf(L"Battery status   : NO SYSTEM BATTERY\n");
    else if (power.BatteryFlag & 8)
        wprintf(L"Battery status   : CHARGING\n");
    else if (power.BatteryFlag & 4)
        wprintf(L"Battery status   : CRITICAL CHARGE\n");
    else if (power.BatteryFlag & 2)
        wprintf(L"Battery status   : LOW CHARGE\n");
    else if (power.BatteryFlag & 1)
        wprintf(L"Battery status   : HIGH CHARGE\n");
    else
        wprintf(L"Battery status   : UNKNOWN\n");

    if (power.BatteryLifeTime == (DWORD)-1)
        wprintf(L"Remaining time   : UNKNOWN\n");
    else
        wprintf(L"Remaining time   : %lu seconds\n", power.BatteryLifeTime);

    if (power.BatteryFullLifeTime == (DWORD)-1)
        wprintf(L"Full battery time: UNKNOWN\n");
    else
        wprintf(L"Full battery time: %lu seconds\n",
            power.BatteryFullLifeTime);
}

static BOOL QueryBatteryValue(
    HANDLE battery,
    ULONG tag,
    BATTERY_QUERY_INFORMATION_LEVEL level,
    void* output,
    DWORD outputSize
)
{
    BATTERY_QUERY_INFORMATION query;
    DWORD returned = 0;

    ZeroMemory(&query, sizeof(query));
    query.BatteryTag = tag;
    query.InformationLevel = level;
    query.AtRate = 0;

    return DeviceIoControl(
        battery,
        IOCTL_BATTERY_QUERY_INFORMATION,
        &query,
        sizeof(query),
        output,
        outputSize,
        &returned,
        NULL
    );
}

static void PrintOneBattery(HANDLE battery, DWORD index)
{
    ULONG waitMs = 0;
    ULONG tag = BATTERY_TAG_INVALID;
    DWORD returned = 0;
    BATTERY_INFORMATION info;
    BATTERY_WAIT_STATUS waitStatus;
    BATTERY_STATUS status;
    WCHAR name[MAX_DEVICE_NAME];
    WCHAR manufacturer[MAX_MANUFACTURER];
    ULONG temperature = 0;
    BOOL hasTemperature;

    if (!DeviceIoControl(
        battery,
        IOCTL_BATTERY_QUERY_TAG,
        &waitMs,
        sizeof(waitMs),
        &tag,
        sizeof(tag),
        &returned,
        NULL
    ) || tag == BATTERY_TAG_INVALID)
    {
        wprintf(L"Battery slot %lu: no battery or tag unavailable.\n", index + 1);
        return;
    }

    ZeroMemory(&info, sizeof(info));
    if (!QueryBatteryValue(
        battery, tag, BatteryInformation, &info, sizeof(info)))
    {
        wprintf(L"Battery slot %lu: information query failed (error %lu).\n",
            index + 1, GetLastError());
        return;
    }

    ZeroMemory(&waitStatus, sizeof(waitStatus));
    waitStatus.BatteryTag = tag;
    waitStatus.Timeout = 0;

    ZeroMemory(&status, sizeof(status));
    if (!DeviceIoControl(
        battery,
        IOCTL_BATTERY_QUERY_STATUS,
        &waitStatus,
        sizeof(waitStatus),
        &status,
        sizeof(status),
        &returned,
        NULL
    ))
    {
        wprintf(L"Battery slot %lu: status query failed (error %lu).\n",
            index + 1, GetLastError());
        return;
    }

    name[0] = L'\0';
    manufacturer[0] = L'\0';
    QueryBatteryValue(battery, tag, BatteryDeviceName, name, sizeof(name));
    QueryBatteryValue(
        battery, tag, BatteryManufactureName,
        manufacturer, sizeof(manufacturer));

    hasTemperature = QueryBatteryValue(
        battery, tag, BatteryTemperature,
        &temperature, sizeof(temperature));

    PrintSeparator();
    wprintf(L"BATTERY SLOT     : %lu\n", index + 1);
    wprintf(L"Battery name     : %ls\n",
        name[0] ? name : L"Not reported");
    wprintf(L"Manufacturer     : %ls\n",
        manufacturer[0] ? manufacturer : L"Not reported");

    if (info.DesignedCapacity != BATTERY_UNKNOWN_CAPACITY)
        wprintf(L"Designed capacity: %lu %ls\n",
            info.DesignedCapacity,
            (info.Capabilities & BATTERY_CAPACITY_RELATIVE) ? L"(relative)" : L"mWh");
    else
        wprintf(L"Designed capacity: Not reported\n");

    if (info.FullChargedCapacity != BATTERY_UNKNOWN_CAPACITY)
        wprintf(L"Full capacity    : %lu %ls\n",
            info.FullChargedCapacity,
            (info.Capabilities & BATTERY_CAPACITY_RELATIVE) ? L"(relative)" : L"mWh");
    else
        wprintf(L"Full capacity    : Not reported\n");

    if (status.Capacity != BATTERY_UNKNOWN_CAPACITY)
    {
        if (info.FullChargedCapacity != 0 &&
            info.FullChargedCapacity != BATTERY_UNKNOWN_CAPACITY)
        {
            double percent =
                ((double)status.Capacity * 100.0) /
                (double)info.FullChargedCapacity;

            if (percent > 100.0)
                percent = 100.0;

            wprintf(L"Current capacity : %lu (%.1f%%)\n",
                status.Capacity, percent);
        }
        else
        {
            wprintf(L"Current capacity : %lu (percentage unavailable)\n",
                status.Capacity);
        }
    }
    else
    {
        wprintf(L"Current capacity : Not reported\n");
    }

    if (status.Voltage != BATTERY_UNKNOWN_VOLTAGE)
        wprintf(L"Voltage          : %lu mV\n", status.Voltage);
    else
        wprintf(L"Voltage          : Not reported\n");

    if (status.Rate != BATTERY_UNKNOWN_RATE)
    {
        if (info.Capabilities & BATTERY_CAPACITY_RELATIVE)
            wprintf(L"Charge/discharge : %ld relative units/hour\n", status.Rate);
        else
            wprintf(L"Charge/discharge : %ld mW\n", status.Rate);
    }
    else
        wprintf(L"Charge/discharge : Not reported\n");

    wprintf(L"Power state      :");
    if (status.PowerState & BATTERY_POWER_ON_LINE)
        wprintf(L" AC connected");
    if (status.PowerState & BATTERY_CHARGING)
        wprintf(L" CHARGING");
    if (status.PowerState & BATTERY_DISCHARGING)
        wprintf(L" DISCHARGING");
    if (status.PowerState & BATTERY_CRITICAL)
        wprintf(L" CRITICAL");
    if (status.PowerState == 0)
        wprintf(L" IDLE / UNKNOWN");
    wprintf(L"\n");

    if (hasTemperature && temperature > 0 && temperature < 10000)
    {
        double celsius = ((double)temperature / 10.0) - 273.15;
        wprintf(L"Temperature      : %.1f C\n", celsius);
    }
    else
    {
        wprintf(L"Temperature      : Not exposed by device\n");
    }

    if (info.CycleCount != 0)
        wprintf(L"Cycle count      : %lu\n", info.CycleCount);
    else
        wprintf(L"Cycle count      : Not reported / zero\n");

    wprintf(L"Capacity mode    : %ls\n",
        (info.Capabilities & BATTERY_CAPACITY_RELATIVE)
            ? L"RELATIVE (not mWh/mW)"
            : L"STANDARD (mWh/mW)");
}

void ScanBatteryInterfaces(void)
{
    HDEVINFO deviceSet;
    DWORD index;
    DWORD found = 0;

    wprintf(L"\n=== INDIVIDUAL BATTERY INTERFACES ===\n");

    deviceSet = SetupDiGetClassDevsW(
        &GUID_DEVCLASS_BATTERY,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (deviceSet == INVALID_HANDLE_VALUE)
    {
        wprintf(L"Battery interface enumeration failed. Error: %lu\n",
            GetLastError());
        return;
    }

    for (index = 0; index < 100; index++)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData;
        DWORD requiredSize = 0;
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail = NULL;
        HANDLE battery = INVALID_HANDLE_VALUE;

        ZeroMemory(&interfaceData, sizeof(interfaceData));
        interfaceData.cbSize = sizeof(interfaceData);

        if (!SetupDiEnumDeviceInterfaces(
            deviceSet,
            NULL,
            &GUID_DEVCLASS_BATTERY,
            index,
            &interfaceData
        ))
        {
            DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_ITEMS)
                wprintf(L"Interface enumeration error: %lu\n", error);
            break;
        }

        SetupDiGetDeviceInterfaceDetailW(
            deviceSet,
            &interfaceData,
            NULL,
            0,
            &requiredSize,
            NULL
        );

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
            requiredSize < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
        {
            continue;
        }

        detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredSize);

        if (detail == NULL)
            continue;

        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (SetupDiGetDeviceInterfaceDetailW(
            deviceSet,
            &interfaceData,
            detail,
            requiredSize,
            NULL,
            NULL
        ))
        {
            /*
               Read access only. This application does not issue
               IOCTL_BATTERY_SET_INFORMATION or write controller registers.
            */
            battery = CreateFileW(
                detail->DevicePath,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (battery != INVALID_HANDLE_VALUE)
            {
                found++;
                PrintOneBattery(battery, index);
                CloseHandle(battery);
            }
            else
            {
                wprintf(L"Battery interface %lu: cannot open (error %lu).\n",
                    index + 1, GetLastError());
            }
        }

        HeapFree(GetProcessHeap(), 0, detail);
    }

    SetupDiDestroyDeviceInfoList(deviceSet);

    if (found == 0)
        wprintf(L"No readable battery interfaces were found.\n");
    else
        wprintf(L"Readable battery interfaces: %lu\n", found);
}

void ScanPresentDevices(void)
{
    HDEVINFO deviceSet;
    SP_DEVINFO_DATA deviceData;
    DWORD index;
    DWORD total = 0;
    DWORD batteries = 0;
    DWORD acpiDevices = 0;
    DWORD controllers = 0;
    DWORD embeddedControllers = 0;
    DWORD busControllers = 0;
    DWORD usbDevices = 0;
    DWORD i2cDevices = 0;
    DWORD smbusDevices = 0;
    DWORD problemDevices = 0;

    wprintf(L"\n=== PNP HARDWARE SCAN ===\n");

    deviceSet = SetupDiGetClassDevsW(
        NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (deviceSet == INVALID_HANDLE_VALUE)
    {
        wprintf(L"Device enumeration failed. Error: %lu\n", GetLastError());
        return;
    }

    for (index = 0; ; index++)
    {
        DEVICE_INFO info;

        ZeroMemory(&deviceData, sizeof(deviceData));
        deviceData.cbSize = sizeof(deviceData);

        if (!SetupDiEnumDeviceInfo(deviceSet, index, &deviceData))
        {
            DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_ITEMS)
                wprintf(L"Enumeration error: %lu\n", error);
            break;
        }

        if (!ReadDeviceInfo(deviceSet, &deviceData, &info))
            continue;

        total++;
        if (info.isBattery) batteries++;
        if (info.isAcpiDevice) acpiDevices++;
        if (info.isPowerController) controllers++;
        if (info.isEmbeddedController) embeddedControllers++;
        if (info.isBusController) busControllers++;
        if (info.isUsbDevice) usbDevices++;
        if (info.isI2cDevice) i2cDevices++;
        if (info.isSmbusDevice) smbusDevices++;
        if (info.hasProblem) problemDevices++;

        if (IsRelevantDevice(&info))
        {
            PrintSeparator();
            PrintDevice(&info);
        }
    }

    SetupDiDestroyDeviceInfoList(deviceSet);
    PrintSeparator();

    wprintf(L"\n=== PNP SCAN RESULTS ===\n");
    wprintf(L"Total device nodes       : %lu\n", total);
    wprintf(L"Battery devices          : %lu\n", batteries);
    wprintf(L"ACPI devices             : %lu\n", acpiDevices);
    wprintf(L"Power-related devices    : %lu\n", controllers);
    wprintf(L"Embedded Controllers     : %lu\n", embeddedControllers);
    wprintf(L"SMBus / I2C devices      : %lu\n", busControllers);
    wprintf(L"Devices with PnP problems: %lu\n", problemDevices);
}



/*
 * Windows power-plan management uses documented PowerCfg APIs.
 * It changes only the active Windows power scheme; it never writes
 * directly to battery-controller registers.
 */
static const GUID GUID_UBM_BALANCED =
{ 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5f, 0xf5, 0x5b, 0x26, 0x0e } };
static const GUID GUID_UBM_PERFORMANCE =
{ 0x8c5e7fda, 0xe8bf, 0x4a96, { 0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c } };
static const GUID GUID_UBM_SAVER =
{ 0xa1841308, 0x3541, 0x4fab, { 0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a } };

static BOOL IsKnownPlan(const GUID* plan, const WCHAR* alias)
{
    if (_wcsicmp(alias, L"balanced") == 0)
        return IsEqualGUID(plan, &GUID_UBM_BALANCED);
    if (_wcsicmp(alias, L"performance") == 0 || _wcsicmp(alias, L"high-performance") == 0)
        return IsEqualGUID(plan, &GUID_UBM_PERFORMANCE);
    if (_wcsicmp(alias, L"saver") == 0 || _wcsicmp(alias, L"power-saver") == 0)
        return IsEqualGUID(plan, &GUID_UBM_SAVER);
    return FALSE;
}

void PrintPowerPlans(void)
{
    DWORD index = 0;
    DWORD size;
    GUID scheme;
    GUID* active = NULL;
    DWORD activeResult = PowerGetActiveScheme(NULL, &active);

    wprintf(L"\n=== WINDOWS POWER PLANS ===\n");
    if (activeResult != ERROR_SUCCESS)
        wprintf(L"Active scheme query failed: %lu\n", activeResult);

    for (;;)
    {
        WCHAR name[256];
        DWORD nameSize = sizeof(name);
        DWORD result;

        ZeroMemory(&scheme, sizeof(scheme));
        size = sizeof(scheme);
        result = PowerEnumerate(
            NULL, NULL, NULL, ACCESS_SCHEME, index,
            (UCHAR*)&scheme, &size
        );

        if (result == ERROR_NO_MORE_ITEMS)
            break;
        if (result != ERROR_SUCCESS)
        {
            wprintf(L"Power scheme enumeration failed: %lu\n", result);
            break;
        }

        ZeroMemory(name, sizeof(name));
        if (PowerReadFriendlyName(
            NULL, &scheme, NULL, NULL, (UCHAR*)name, &nameSize
        ) != ERROR_SUCCESS || name[0] == L'\0')
        {
            wcscpy_s(name, sizeof(name) / sizeof(name[0]), L"Unnamed power plan");
        }

        wprintf(L"%c %ls\n",
            (active != NULL && IsEqualGUID(active, &scheme)) ? L'*' : L' ',
            name);
        index++;
    }

    if (index == 0)
        wprintf(L"No power schemes were returned by Windows.\n");

    if (active != NULL)
        LocalFree(active);
}

BOOL SetPowerPlanByName(const WCHAR* name)
{
    DWORD index = 0;
    DWORD size;
    GUID scheme;
    BOOL found = FALSE;
    DWORD result = ERROR_NOT_FOUND;

    if (name == NULL || name[0] == L'\0')
        return FALSE;

    for (;;)
    {
        WCHAR friendlyName[256];
        DWORD nameSize = sizeof(friendlyName);

        ZeroMemory(&scheme, sizeof(scheme));
        size = sizeof(scheme);
        result = PowerEnumerate(
            NULL, NULL, NULL, ACCESS_SCHEME, index,
            (UCHAR*)&scheme, &size
        );

        if (result == ERROR_NO_MORE_ITEMS)
            break;
        if (result != ERROR_SUCCESS)
        {
            wprintf(L"Cannot enumerate power plans: %lu\n", result);
            return FALSE;
        }

        ZeroMemory(friendlyName, sizeof(friendlyName));
        PowerReadFriendlyName(
            NULL, &scheme, NULL, NULL,
            (UCHAR*)friendlyName, &nameSize
        );

        if (IsKnownPlan(&scheme, name) ||
            _wcsicmp(friendlyName, name) == 0)
        {
            found = TRUE;
            break;
        }
        index++;
    }

    if (!found)
    {
        wprintf(L"Requested power plan was not found: %ls\n", name);
        return FALSE;
    }

    result = PowerSetActiveScheme(NULL, &scheme);
    if (result != ERROR_SUCCESS)
    {
        wprintf(L"Windows rejected the power-plan change: %lu\n", result);
        return FALSE;
    }

    wprintf(L"Windows power plan changed successfully.\n");
    return TRUE;
}

int wmain(int argc, wchar_t** argv)
{
    wprintf(
        L"\n"
        L"============================================================\n"
        L"                 UNIVERSAL BATTERY MANAGER\n"
        L"============================================================\n"
    );
    wprintf(L"Version: %ls\n", APP_VERSION);
    wprintf(L"Mode: automatic detection and read-only battery monitoring\n");
    wprintf(L"Polling interval: %u seconds\n",
        BATTERY_POLL_INTERVAL_MS / 1000);
    wprintf(L"Hardware writes: DISABLED\n");
    wprintf(L"Windows power-plan API: AVAILABLE\n");

    if (argc >= 2 && _wcsicmp(argv[1], L"--list-plans") == 0)
    {
        PrintPowerPlans();
        return 0;
    }
    if (argc >= 3 && _wcsicmp(argv[1], L"--set-plan") == 0)
    {
        if (!SetPowerPlanByName(argv[2]))
            return 2;
        PrintPowerPlans();
        return 0;
    }
    if (argc >= 2 && _wcsicmp(argv[1], L"--help") == 0)
    {
        wprintf(L"Options:\\n  --list-plans\\n  --set-plan balanced|performance|saver\\n");
        return 0;
    }

    PrintPowerPlans();
    ScanPresentDevices();

    wprintf(L"\nPress Q to stop monitoring.\n");

    for (;;)
    {
        PrintBatteryStatus();
        ScanBatteryInterfaces();

        wprintf(L"\nNext refresh in %u seconds. Press Q to exit.\n",
            BATTERY_POLL_INTERVAL_MS / 1000);

        {
            DWORD elapsed = 0;

            while (elapsed < BATTERY_POLL_INTERVAL_MS)
            {
                if (_kbhit())
                {
                    int key = _getch();
                    if (key == 'q' || key == 'Q')
                    {
                        wprintf(L"\nMonitoring stopped by user.\n");
                        return 0;
                    }
                }

                Sleep(100);
                elapsed += 100;
            }
        }
    }
}
