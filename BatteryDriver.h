#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <cfg.h>
#include <devguid.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

#define APP_NAME L"Universal Battery Manager"
#define APP_VERSION L"0.2.0"

#define MAX_INSTANCE_ID 1024
#define MAX_DEVICE_NAME 512
#define MAX_CLASS_NAME 128
#define MAX_MANUFACTURER 256
#define MAX_HARDWARE_IDS 2048

typedef struct _DEVICE_INFO
{
    WCHAR instanceId[MAX_INSTANCE_ID];
    WCHAR friendlyName[MAX_DEVICE_NAME];
    WCHAR description[MAX_DEVICE_NAME];
    WCHAR className[MAX_CLASS_NAME];
    WCHAR manufacturer[MAX_MANUFACTURER];
    WCHAR hardwareIds[MAX_HARDWARE_IDS];

    DWORD devNodeStatus;
    ULONG problemCode;

    BOOL hasProblem;
    BOOL isPresent;
    BOOL isBattery;
    BOOL isAcpiDevice;
    BOOL isPowerController;
    BOOL isEmbeddedController;
    BOOL isBusController;

} DEVICE_INFO, *PDEVICE_INFO;

BOOL ContainsTextInsensitive(
    const WCHAR* text,
    const WCHAR* part
);

BOOL ReadDeviceInfo(
    HDEVINFO deviceSet,
    SP_DEVINFO_DATA* deviceData,
    DEVICE_INFO* output
);

void ScanPresentDevices(void);

void PrintDevice(
    const DEVICE_INFO* device
);

void PrintBatteryStatus(void);

BOOL IsRelevantDevice(
    const DEVICE_INFO* device
);
