#pragma once
#ifndef UBM_BATTERY_ADAPTERS_H
#define UBM_BATTERY_ADAPTERS_H
#include <windows.h>
#include <stddef.h>
#define UBM_ADAPTER_ABI_VERSION 1
typedef enum _UBM_ADAPTER_PROTOCOL {
 UBM_PROTOCOL_WINDOWS_BATTERY_CLASS=1, UBM_PROTOCOL_ACPI_CMB=2,
 UBM_PROTOCOL_SMBUS=3, UBM_PROTOCOL_I2C=4, UBM_PROTOCOL_USB_HID_POWER=5
} UBM_ADAPTER_PROTOCOL;
typedef struct _UBM_ADAPTER_DESCRIPTOR {
 DWORD abiVersion; UBM_ADAPTER_PROTOCOL protocol;
 const WCHAR* name; const WCHAR* detection; const WCHAR* currentSupport;
 BOOL canReadBatteryTelemetry; BOOL canChangeHardwareSettings;
} UBM_ADAPTER_DESCRIPTOR;
const UBM_ADAPTER_DESCRIPTOR* UBM_GetAdapterCatalog(size_t* count);
void UBM_PrintAdapterCatalog(void);
#endif
