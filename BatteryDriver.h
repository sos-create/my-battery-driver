#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <batclass.h>
#include <poclass.h>

#define BATTERY_TAG 'ttaB'

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    PVOID ClassHandle; 
    ULONG BatteryTag;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

NTSTATUS BatteryMiniQueryTag(PVOID Context, PULONG Tag);
NTSTATUS BatteryMiniQueryInformation(PVOID Context, ULONG Tag, BATTERY_QUERY_INFORMATION_TYPE InformationType, PVOID Buffer, PULONG BufferLength);
NTSTATUS BatteryMiniQueryStatus(PVOID Context, ULONG Tag, PBATTERY_STATUS Status);
NTSTATUS BatteryMiniSetInformation(PVOID Context, ULONG Tag, BATTERY_SET_INFORMATION_TYPE InformationType, PVOID Buffer);
NTSTATUS BatteryMiniSetStatusNotify(PVOID Context, ULONG Tag, PBATTERY_STATUS Status);
NTSTATUS BatteryMiniDisableStatusNotify(PVOID Context);
