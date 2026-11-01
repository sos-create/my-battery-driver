#include "BatteryAdapters.h"
#include <stdio.h>
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
