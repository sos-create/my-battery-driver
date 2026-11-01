#include "BatteryAdapters.h"
#include <stdio.h>
#include <strsafe.h>
#include <wchar.h>
static const UBM_ADAPTER_DESCRIPTOR g_adapters[] = {
 {UBM_ADAPTER_ABI_VERSION,UBM_PROTOCOL_WINDOWS_BATTERY_CLASS,L"Windows Battery Class",L"Battery-class PnP interface and IOCTL_BATTERY_*",L"Supported: read-only telemetry",TRUE,FALSE},
 {UBM_ADAPTER_ABI_VERSION,UBM_PROTOCOL_ACPI_CMB,L"ACPI Control Method Battery",L"ACPI PNP0C0A device identification",L"Detected through PnP; readings use Windows Battery Class",FALSE,FALSE},
 {UBM_ADAPTER_ABI_VERSION,UBM_PROTOCOL_SMBUS,L"SMBus controller",L"PnP hardware IDs and device descriptions",L"Discovery only; vendor protocol required",FALSE,FALSE},
 {UBM_ADAPTER_ABI_VERSION,UBM_PROTOCOL_I2C,L"I2C controller",L"PnP hardware IDs and device descriptions",L"Discovery only; device-specific protocol required",FALSE,FALSE},
 {UBM_ADAPTER_ABI_VERSION,UBM_PROTOCOL_USB_HID_POWER,L"USB HID Power Device",L"USB/HID PnP identification",L"Discovery only in this release",FALSE,FALSE}
};
const UBM_ADAPTER_DESCRIPTOR* UBM_GetAdapterCatalog(size_t* count) {
 if(count) *count=sizeof(g_adapters)/sizeof(g_adapters[0]); return g_adapters;
}
void UBM_PrintAdapterCatalog(void) {
 size_t n=0,i; const UBM_ADAPTER_DESCRIPTOR* a=UBM_GetAdapterCatalog(&n);
 wprintf(L"\n=== BUILT-IN ADAPTER CATALOG ===\n");
 for(i=0;i<n;++i) {
  wprintf(L"%ls\n  Detection: %ls\n  Support: %ls\n",a[i].name,a[i].detection,a[i].currentSupport);
  wprintf(L"  Read telemetry: %ls | Hardware control: %ls\n",a[i].canReadBatteryTelemetry?L"YES":L"NO",a[i].canChangeHardwareSettings?L"YES":L"NO");
 }
}


#define UBM_MAX_EXTERNAL_ADAPTERS 16
static HMODULE g_adapterModules[UBM_MAX_EXTERNAL_ADAPTERS];
static const UBM_EXTERNAL_ADAPTER* g_externalAdapters[UBM_MAX_EXTERNAL_ADAPTERS];
static size_t g_externalAdapterCount = 0;
static BOOL g_externalAdaptersLoaded = FALSE;

size_t UBM_LoadExternalAdapters(void)
{
    WCHAR modulePath[MAX_PATH];
    WCHAR searchPath[MAX_PATH];
    WCHAR dllPath[MAX_PATH];
    WCHAR* slash;
    WIN32_FIND_DATAW findData;
    HANDLE findHandle;

    if (g_externalAdaptersLoaded)
        return g_externalAdapterCount;
    g_externalAdaptersLoaded = TRUE;

    if (!GetModuleFileNameW(NULL, modulePath, (sizeof(modulePath)/sizeof(modulePath[0]))))
        return 0;

    slash = wcsrchr(modulePath, L'\\');
    if (slash == NULL)
        return 0;
    *slash = L'\0';

    if (FAILED(StringCchPrintfW(
        searchPath, (sizeof(searchPath)/sizeof(searchPath[0])), L"%ls\\Adapters\\*.dll", modulePath)))
        return 0;

    findHandle = FindFirstFileW(searchPath, &findData);
    if (findHandle == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        const UBM_EXTERNAL_ADAPTER* adapter;
        UBM_GET_ADAPTER_PLUGIN getAdapter;
        HMODULE module;

        if (g_externalAdapterCount >= UBM_MAX_EXTERNAL_ADAPTERS)
            break;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        if (FAILED(StringCchPrintfW(
            dllPath, (sizeof(dllPath)/sizeof(dllPath[0])), L"%ls\\Adapters\\%ls",
            modulePath, findData.cFileName)))
            continue;

        module = LoadLibraryExW(
            dllPath, NULL,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32
        );
        if (module == NULL)
            continue;

        getAdapter = (UBM_GET_ADAPTER_PLUGIN)GetProcAddress(
            module, "UBM_GetAdapterPlugin"
        );
#if defined(_M_IX86)
        if (getAdapter == NULL)
            getAdapter = (UBM_GET_ADAPTER_PLUGIN)GetProcAddress(module, "_UBM_GetAdapterPlugin@4");
        if (getAdapter == NULL)
            getAdapter = (UBM_GET_ADAPTER_PLUGIN)GetProcAddress(module, "UBM_GetAdapterPlugin@4");
#endif
        if (getAdapter == NULL)
        {
            FreeLibrary(module);
            continue;
        }

        adapter = getAdapter(UBM_EXTERNAL_ADAPTER_ABI);
        if (adapter == NULL ||
            adapter->structSize < sizeof(UBM_EXTERNAL_ADAPTER) ||
            adapter->abiVersion != UBM_EXTERNAL_ADAPTER_ABI ||
            adapter->name == NULL ||
            adapter->CanHandle == NULL ||
            adapter->QueryTelemetry == NULL)
        {
            FreeLibrary(module);
            continue;
        }

        g_adapterModules[g_externalAdapterCount] = module;
        g_externalAdapters[g_externalAdapterCount] = adapter;
        g_externalAdapterCount++;
    }
    while (FindNextFileW(findHandle, &findData));

    FindClose(findHandle);
    return g_externalAdapterCount;
}

size_t UBM_GetExternalAdapterCount(void)
{
    return g_externalAdapterCount;
}

const WCHAR* UBM_GetExternalAdapterName(size_t index)
{
    if (index >= g_externalAdapterCount)
        return L"";
    return g_externalAdapters[index]->name;
}

BOOL UBM_QueryExternalAdapters(
    const UBM_DEVICE_DESCRIPTOR* device,
    UBM_TELEMETRY* telemetry,
    const WCHAR** adapterName)
{
    size_t i;

    if (device == NULL || telemetry == NULL)
        return FALSE;

    ZeroMemory(telemetry, sizeof(*telemetry));
    if (adapterName != NULL)
        *adapterName = NULL;

    for (i = 0; i < g_externalAdapterCount; ++i)
    {
        const UBM_EXTERNAL_ADAPTER* adapter = g_externalAdapters[i];
        if (adapter->CanHandle(device) &&
            adapter->QueryTelemetry(device, telemetry))
        {
            if (adapterName != NULL)
                *adapterName = adapter->name;
            return TRUE;
        }
        ZeroMemory(telemetry, sizeof(*telemetry));
    }
    return FALSE;
}
