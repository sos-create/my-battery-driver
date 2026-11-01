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

    ZeroMemory(
        buffer,
        bufferChars * sizeof(WCHAR)
    );

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

    ZeroMemory(
        buffer,
        bufferChars * sizeof(WCHAR)
    );

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
        deviceSet,
        deviceData,
        SPDRP_FRIENDLYNAME,
        output->friendlyName,
        MAX_DEVICE_NAME
    );

    if (output->friendlyName[0] == L'\0')
    {
        ReadRegistryString(
            deviceSet,
            deviceData,
            SPDRP_DEVICEDESC,
            output->friendlyName,
            MAX_DEVICE_NAME
        );
    }

    ReadRegistryString(
        deviceSet,
        deviceData,
        SPDRP_DEVICEDESC,
        output->description,
        MAX_DEVICE_NAME
    );

    ReadRegistryString(
        deviceSet,
        deviceData,
        SPDRP_CLASS,
        output->className,
        MAX_CLASS_NAME
    );

    ReadRegistryString(
        deviceSet,
        deviceData,
        SPDRP_MFG,
        output->manufacturer,
        MAX_MANUFACTURER
    );

    ReadHardwareIds(
        deviceSet,
        deviceData,
        output->hardwareIds,
        MAX_HARDWARE_IDS
    );

    /*
       The device was enumerated using DIGCF_PRESENT.
       Therefore it is currently present in the device list.
    */

    output->isPresent = TRUE;

    /*
       Obtain PnP status and problem code.
    */

    result = CM_Get_DevNode_Status(
        &status,
        &problem,
        deviceData->DevInst,
        0
    );

    if (result == CR_SUCCESS)
    {
        output->devNodeStatus = status;
        output->problemCode = problem;
        output->hasProblem = (problem != 0);
    }

    /*
       Battery detection.
    */

    output->isBattery =
        ContainsTextInsensitive(
            output->className,
            L"Battery"
        ) ||
        ContainsTextInsensitive(
            output->instanceId,
            L"PNP0C0A"
        ) ||
        ContainsTextInsensitive(
            output->friendlyName,
            L"battery"
        ) ||
        ContainsTextInsensitive(
            output->friendlyName,
            L"батар"
        );

    /*
       ACPI detection.
    */

    output->isAcpiDevice =
        ContainsTextInsensitive(
            output->instanceId,
            L"ACPI\\"
        );

    /*
       Embedded Controller detection.
    */

    output->isEmbeddedController =
        ContainsTextInsensitive(
            output->instanceId,
            L"PNP0C09"
        ) ||
        ContainsTextInsensitive(
            output->friendlyName,
            L"Embedded Controller"
        ) ||
        ContainsTextInsensitive(
            output->description,
            L"Embedded Controller"
        );

    /*
       SMBus / I2C detection.
    */

    output->isBusController =
        ContainsTextInsensitive(
            output->friendlyName,
            L"SMBus"
        ) ||
        ContainsTextInsensitive(
            output->friendlyName,
            L"I2C"
        ) ||
        ContainsTextInsensitive(
            output->description,
            L"SMBus"
        ) ||
        ContainsTextInsensitive(
            output->description,
            L"I2C"
        );

    /*
       Power-related device detection.
    */

    output->isPowerController =
        ContainsTextInsensitive(
            output->instanceId,
            L"PNP0C09"
        ) ||
        ContainsTextInsensitive(
            output->instanceId,
            L"PNP0C0C"
        ) ||
        output->isEmbeddedController ||
        output->isBusController ||
        ContainsTextInsensitive(
            output->friendlyName,
            L"Power Management"
        );

    return TRUE;
}

BOOL IsRelevantDevice(
    const DEVICE_INFO* device
)
{
    if (device == NULL)
        return FALSE;

    return
        device->isBattery ||
        device->isAcpiDevice ||
        device->isPowerController ||
        device->hasProblem;
}

void PrintDevice(
    const DEVICE_INFO* device
)
{
    if (device == NULL)
        return;

    wprintf(
        L"Name             : %ls\n",
        device->friendlyName[0]
            ? device->friendlyName
            : L"Unknown"
    );

    wprintf(
        L"Class            : %ls\n",
        device->className[0]
            ? device->className
            : L"Unknown"
    );

    wprintf(
        L"Manufacturer     : %ls\n",
        device->manufacturer[0]
            ? device->manufacturer
            : L"Unknown"
    );

    wprintf(
        L"Instance ID      : %ls\n",
        device->instanceId
    );

    wprintf(
        L"Hardware IDs     : %ls\n",
        device->hardwareIds[0]
            ? device->hardwareIds
            : L"Unknown"
    );

    wprintf(
        L"Present          : %ls\n",
        device->isPresent ? L"YES" : L"NO"
    );

    wprintf(
        L"Battery          : %ls\n",
        device->isBattery ? L"YES" : L"NO"
    );

    wprintf(
        L"ACPI             : %ls\n",
        device->isAcpiDevice ? L"YES" : L"NO"
    );

    wprintf(
        L"Embedded Control : %ls\n",
        device->isEmbeddedController ? L"YES" : L"NO"
    );

    wprintf(
        L"SMBus / I2C      : %ls\n",
        device->isBusController ? L"YES" : L"NO"
    );

    if (device->hasProblem)
    {
        wprintf(
            L"PnP ERROR        : %lu\n",
            device->problemCode
        );
    }
    else
    {
        wprintf(L"PnP ERROR        : NONE\n");
    }
}

void PrintBatteryStatus(void)
{
    SYSTEM_POWER_STATUS power;

    wprintf(L"\n=== WINDOWS BATTERY STATUS ===\n");

    ZeroMemory(&power, sizeof(power));

    if (!GetSystemPowerStatus(&power))
    {
        wprintf(
            L"Cannot obtain power status. Error: %lu\n",
            GetLastError()
        );

        return;
    }

    if (power.ACLineStatus == 1)
        wprintf(L"AC adapter       : CONNECTED\n");
    else if (power.ACLineStatus == 0)
        wprintf(L"AC adapter       : DISCONNECTED\n");
    else
        wprintf(L"AC adapter       : UNKNOWN\n");

    if (power.BatteryLifePercent == 255)
    {
        wprintf(L"Battery charge   : UNKNOWN\n");
    }
    else
    {
        wprintf(
            L"Battery charge   : %u%%\n",
            (unsigned)power.BatteryLifePercent
        );
    }

    if (power.BatteryFlag == 255)
    {
        wprintf(L"Battery status   : UNKNOWN\n");
    }
    else if (power.BatteryFlag & 128)
    {
        wprintf(L"Battery status   : NO SYSTEM BATTERY\n");
    }
    else if (power.BatteryFlag & 8)
    {
        wprintf(L"Battery status   : CHARGING\n");
    }
    else if (power.BatteryFlag & 4)
    {
        wprintf(L"Battery status   : CRITICAL CHARGE\n");
    }
    else if (power.BatteryFlag & 2)
    {
        wprintf(L"Battery status   : LOW CHARGE\n");
    }
    else if (power.BatteryFlag & 1)
    {
        wprintf(L"Battery status   : HIGH CHARGE\n");
    }
    else
    {
        wprintf(L"Battery status   : UNKNOWN\n");
    }

    wprintf(
        L"Battery flags    : 0x%02X\n",
        (unsigned)power.BatteryFlag
    );

    if (power.BatteryLifeTime == (DWORD)-1)
    {
        wprintf(L"Remaining time   : UNKNOWN\n");
    }
    else
    {
        wprintf(
            L"Remaining time   : %lu seconds\n",
            power.BatteryLifeTime
        );
    }

    if (power.BatteryFullLifeTime == (DWORD)-1)
    {
        wprintf(L"Full battery time: UNKNOWN\n");
    }
    else
    {
        wprintf(
            L"Full battery time: %lu seconds\n",
            power.BatteryFullLifeTime
        );
    }
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
    DWORD problemDevices = 0;

    wprintf(L"\n=== HARDWARE SCAN ===\n");

    /*
       Enumerate only devices currently present.
    */

    deviceSet = SetupDiGetClassDevsW(
        NULL,
        NULL,
        NULL,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (deviceSet == INVALID_HANDLE_VALUE)
    {
        wprintf(
            L"Device enumeration failed. Error: %lu\n",
            GetLastError()
        );

        return;
    }

    for (index = 0; ; index++)
    {
        DEVICE_INFO info;

        ZeroMemory(
            &deviceData,
            sizeof(deviceData)
        );

        deviceData.cbSize = sizeof(deviceData);

        if (!SetupDiEnumDeviceInfo(
            deviceSet,
            index,
            &deviceData
        ))
        {
            DWORD error = GetLastError();

            if (error != ERROR_NO_MORE_ITEMS)
            {
                wprintf(
                    L"Enumeration error: %lu\n",
                    error
                );
            }

            break;
        }

        if (!ReadDeviceInfo(
            deviceSet,
            &deviceData,
            &info
        ))
        {
            continue;
        }

        total++;

        if (info.isBattery)
            batteries++;

        if (info.isAcpiDevice)
            acpiDevices++;

        if (info.isPowerController)
            controllers++;

        if (info.isEmbeddedController)
            embeddedControllers++;

        if (info.isBusController)
            busControllers++;

        if (info.hasProblem)
            problemDevices++;

        if (IsRelevantDevice(&info))
        {
            PrintSeparator();
            PrintDevice(&info);
        }
    }

    SetupDiDestroyDeviceInfoList(deviceSet);

    PrintSeparator();

    wprintf(L"\n=== SCAN RESULTS ===\n");

    wprintf(
        L"Total device nodes       : %lu\n",
        total
    );

    wprintf(
        L"Battery devices          : %lu\n",
        batteries
    );

    wprintf(
        L"ACPI devices             : %lu\n",
        acpiDevices
    );

    wprintf(
        L"Power-related devices    : %lu\n",
        controllers
    );

    wprintf(
        L"Embedded Controllers     : %lu\n",
        embeddedControllers
    );

    wprintf(
        L"SMBus / I2C devices      : %lu\n",
        busControllers
    );

    wprintf(
        L"Devices with PnP problems: %lu\n",
        problemDevices
    );
}

int wmain(void)
{
    wprintf(
        L"\n"
        L"============================================\n"
        L"       UNIVERSAL BATTERY MANAGER\n"
        L"============================================\n"
    );

    wprintf(
        L"Version: %ls\n",
        APP_VERSION
    );

    wprintf(
        L"Windows 10 hardware diagnostic\n"
    );

    wprintf(
        L"No hardware registers will be modified.\n"
    );

    PrintBatteryStatus();

    ScanPresentDevices();

    wprintf(L"\nDiagnostic finished.\n");

    wprintf(L"Press ENTER to close...");

    (void)getwchar();

    return 0;
}
