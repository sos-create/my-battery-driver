#include "BatteryDriver.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_DRIVER_CONFIG_INIT(&config, NULL);
    config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
    return status;
}

NTSTATUS BatteryEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    PDEVICE_CONTEXT devContext;
    BATTERY_MINIPORT_INFO miniportInfo;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) return status;

    devContext = GetDeviceContext(device);
    devContext->Device = device;
    devContext->BatteryTag = 1;

    RtlZeroMemory(&miniportInfo, sizeof(miniportInfo));
    miniportInfo.MajorVersion = BATTERY_CLASS_MAJOR_VERSION;
    miniportInfo.MinorVersion = BATTERY_CLASS_MINOR_VERSION;
    miniportInfo.Context = devContext;
    miniportInfo.QueryTag = BatteryMiniQueryTag;
    miniportInfo.QueryInformation = BatteryMiniQueryInformation;
    miniportInfo.QueryStatus = BatteryMiniQueryStatus;
    miniportInfo.SetInformation = BatteryMiniSetInformation;
    miniportInfo.SetStatusNotify = BatteryMiniSetStatusNotify;
    miniportInfo.DisableStatusNotify = BatteryMiniDisableStatusNotify;

    status = BatteryClassInitializeDevice(&miniportInfo, &devContext->ClassHandle);
    return status;
}

NTSTATUS BatteryMiniQueryTag(PVOID Context, PULONG Tag) {
    *Tag = 1; 
    return STATUS_SUCCESS;
}

NTSTATUS BatteryMiniQueryInformation(PVOID Context, ULONG Tag, BATTERY_QUERY_INFORMATION_TYPE InformationType, PVOID Buffer, PULONG BufferLength) {
    switch (InformationType) {
        case BatteryInformation: {
            if (*BufferLength < sizeof(BATTERY_INFORMATION)) {
                *BufferLength = sizeof(BATTERY_INFORMATION);
                return STATUS_BUFFER_TOO_SMALL;
            }
            PBATTERY_INFORMATION info = (PBATTERY_INFORMATION)Buffer;
            RtlZeroMemory(info, sizeof(BATTERY_INFORMATION));
            info->Capabilities = BATTERY_CAPACITY_RELATIVE | BATTERY_IS_SHORT_TERM;
            info->Technology = 1; 
            info->Chemistry[0] = 'L'; info->Chemistry[1] = 'I'; info->Chemistry[2] = 'O'; info->Chemistry[3] = 'N';
            info->DesignedCapacity = 30400; 
            info->FullChargedCapacity = 30400;
            *BufferLength = sizeof(BATTERY_INFORMATION);
            return STATUS_SUCCESS;
        }
        case BatteryDeviceName: {
            const WCHAR name[] = L"NV-3378107-2P";
            ULONG nameBytes = sizeof(name);
            if (*BufferLength < nameBytes) {
                *BufferLength = nameBytes;
                return STATUS_BUFFER_TOO_SMALL;
            }
            RtlCopyMemory(Buffer, name, nameBytes);
            *BufferLength = nameBytes;
            return STATUS_SUCCESS;
        }
        default:
            *BufferLength = 0;
            return STATUS_INVALID_PARAMETER;
    }
}

NTSTATUS BatteryMiniQueryStatus(PVOID Context, ULONG Tag, PBATTERY_STATUS Status) {
    RtlZeroMemory(Status, sizeof(BATTERY_STATUS));
    Status->PowerState = BATTERY_DISCHARGING; 
    Status->Capacity = 50; 
    Status->Voltage = 3800; 
    Status->Rate = 10000; 
    return STATUS_SUCCESS;
}

NTSTATUS BatteryMiniSetInformation(PVOID Context, ULONG Tag, BATTERY_SET_INFORMATION_TYPE InformationType, PVOID Buffer) {
    return STATUS_SUCCESS;
}
NTSTATUS BatteryMiniSetStatusNotify(PVOID Context, ULONG Tag, PBATTERY_STATUS Status) {
    return STATUS_SUCCESS;
}
NTSTATUS BatteryMiniDisableStatusNotify(PVOID Context) {
    return STATUS_SUCCESS;
}
