#include "BatteryDriver.h"

// Точка входа драйвера
NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
) {
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    KdPrint(("BatteryDriver: DriverEntry called\n"));

    WDF_DRIVER_CONFIG_INIT(&config, NULL);
    config.EvtDriverDeviceAdd = NULL; // Обработка добавления устройства вручную
    config.DriverInitFlags |= WdfDriverInitNonPnpDriver;

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        KdPrint(("BatteryDriver: WdfDriverCreate failed 0x%x\n", status));
        return status;
    }

    return STATUS_SUCCESS;
}

// Эта функция вызывается, когда PnP-менеджер добавляет устройство
NTSTATUS BatteryEvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
) {
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    PDEVICE_CONTEXT devContext;
    BATTERY_MINIPORT_INFO miniportInfo;
    UNICODE_STRING deviceName;
    WDFMEMORY deviceNameMemory;

    UNREFERENCED_PARAMETER(Driver);

    KdPrint(("BatteryDriver: DeviceAdd called\n"));

    // 1. Создаем устройство
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("BatteryDriver: WdfDeviceCreate failed 0x%x\n", status));
        return status;
    }

    devContext = GetDeviceContext(device);
    devContext->Device = device;
    devContext->BatteryTag = 1;

    // 2. Инициализируем структуру BATTERY_MINIPORT_INFO
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

    // 3. Регистрируем драйвер в классе батарей
    status = BatteryClassInitializeDevice(&miniportInfo, &devContext->ClassHandle);
    if (!NT_SUCCESS(status)) {
        KdPrint(("BatteryDriver: BatteryClassInitializeDevice failed 0x%x\n", status));
        return status;
    }

    KdPrint(("BatteryDriver: Device initialized successfully\n"));
    return STATUS_SUCCESS;
}

// --- Реализация обязательных функций мини-класса ---

NTSTATUS BatteryMiniQueryTag(
    PVOID Context,
    PULONG Tag
) {
    PDEVICE_CONTEXT devContext = (PDEVICE_CONTEXT)Context;
    UNREFERENCED_PARAMETER(devContext);
    
    // Возвращаем уникальный тег для этой батареи
    *Tag = 1; 
    KdPrint(("BatteryDriver: QueryTag called, Tag=%lu\n", *Tag));
    return STATUS_SUCCESS;
}

NTSTATUS BatteryMiniQueryInformation(
    PVOID Context,
    ULONG Tag,
    BATTERY_QUERY_INFORMATION_TYPE InformationType,
    PVOID Buffer,
    PULONG BufferLength
) {
    PDEVICE_CONTEXT devContext = (PDEVICE_CONTEXT)Context;
    UNREFERENCED_PARAMETER(devContext);
    UNREFERENCED_PARAMETER(Tag);

    KdPrint(("BatteryDriver: QueryInformation called, Type=%d\n", InformationType));

    // Здесь мы сообщаем Windows информацию о батарее
    // ВАЖНО: Это симуляция. Для реальных данных нужно читать EC.
    switch (InformationType) {
        case BatteryInformation: {
            if (*BufferLength < sizeof(BATTERY_INFORMATION)) {
                *BufferLength = sizeof(BATTERY_INFORMATION);
                return STATUS_BUFFER_TOO_SMALL;
            }
            PBATTERY_INFORMATION info = (PBATTERY_INFORMATION)Buffer;
            RtlZeroMemory(info, sizeof(BATTERY_INFORMATION));
            
            info->Capabilities = BATTERY_CAPACITY_RELATIVE | BATTERY_IS_SHORT_TERM;
            info->Technology = 1; // Secondary battery
            info->Chemistry[0] = 'L'; info->Chemistry[1] = 'I'; info->Chemistry[2] = 'O'; info->Chemistry[3] = 'N';
            info->DesignedCapacity = 30400; // 30.4 Wh в mWh
            info->FullChargedCapacity = 30400;
            info->DefaultAlert1 = 3040;
            info->DefaultAlert2 = 1520;
            info->CriticalBias = 0;
            info->CycleCount = 1;
            
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

NTSTATUS BatteryMiniQueryStatus(
    PVOID Context,
    ULONG Tag,
    PBATTERY_STATUS Status
) {
    PDEVICE_CONTEXT devContext = (PDEVICE_CONTEXT)Context;
    UNREFERENCED_PARAMETER(devContext);
    UNREFERENCED_PARAMETER(Tag);

    // ВНИМАНИЕ: Здесь должны быть реальные данные с контроллера EC!
    // Сейчас здесь симуляция: 50% заряд, 3.8V, разряжается.
    
    RtlZeroMemory(Status, sizeof(BATTERY_STATUS));
    Status->PowerState = BATTERY_DISCHARGING; // Питание от батареи
    Status->Capacity = 50; // 50% заряда
    Status->Voltage = 3800; // 3.8V в мВ
    Status->Rate = 10000; // Скорость разряда в мВт

    KdPrint(("BatteryDriver: QueryStatus called, Capacity=%lu%%\n", Status->Capacity));
    return STATUS_SUCCESS;
}

NTSTATUS BatteryMiniSetInformation(
    PVOID Context,
    ULONG Tag,
    BATTERY_SET_INFORMATION_TYPE InformationType,
    PVOID Buffer
) {
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Tag);
    UNREFERENCED_PARAMETER(InformationType);
    UNREFERENCED_PARAMETER(Buffer);
    return STATUS_SUCCESS;
}

NTSTATUS BatteryMiniSetStatusNotify(
    PVOID Context,
    ULONG Tag,
    PBATTERY_STATUS Status
) {
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Tag);
    UNREFERENCED_PARAMETER(Status);
    return STATUS_SUCCESS;
}

NTSTATUS BatteryMiniDisableStatusNotify(
    PVOID Context
) {
    UNREFERENCED_PARAMETER(Context);
    return STATUS_SUCCESS;
}
