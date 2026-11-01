#pragma once
#ifndef UBM_EXTERNAL_ADAPTER_SDK_H
#define UBM_EXTERNAL_ADAPTER_SDK_H
#include <windows.h>
#define UBM_EXTERNAL_ADAPTER_ABI 1
#define UBM_TELEMETRY_PERCENT 0x0001
#define UBM_TELEMETRY_TEMPERATURE 0x0002
#define UBM_TELEMETRY_VOLTAGE 0x0004
#define UBM_TELEMETRY_POWER_RATE 0x0008
#define UBM_TELEMETRY_CYCLE_COUNT 0x0010
typedef struct UBM_DEVICE_DESCRIPTOR {
 WCHAR instanceId[512];
 WCHAR hardwareIds[2048];
 WCHAR description[512];
} UBM_DEVICE_DESCRIPTOR;
typedef struct UBM_TELEMETRY {
 DWORD validFlags;
 double chargePercent;
 double temperatureC;
 DWORD voltageMillivolts;
 LONG powerRateMilliwatts;
 DWORD cycleCount;
} UBM_TELEMETRY;
typedef struct UBM_EXTERNAL_ADAPTER {
 DWORD structSize;
 DWORD abiVersion;
 const WCHAR* name;
 BOOL (WINAPI *CanHandle)(const UBM_DEVICE_DESCRIPTOR* device);
 BOOL (WINAPI *QueryTelemetry)(const UBM_DEVICE_DESCRIPTOR* device, UBM_TELEMETRY* telemetry);
} UBM_EXTERNAL_ADAPTER;
typedef const UBM_EXTERNAL_ADAPTER* (WINAPI *UBM_GET_ADAPTER_PLUGIN)(DWORD hostAbiVersion);
#endif
