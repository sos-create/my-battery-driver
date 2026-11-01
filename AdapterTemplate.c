#include "BatteryAdapterSdk.h"

/* Safe starter adapter: it deliberately does not claim any hardware. */
static BOOL WINAPI TemplateCanHandle(const UBM_DEVICE_DESCRIPTOR* device)
{
    (void)device;
    return FALSE;
}

static BOOL WINAPI TemplateQuery(
    const UBM_DEVICE_DESCRIPTOR* device,
    UBM_TELEMETRY* telemetry)
{
    (void)device;
    if (telemetry != NULL)
        ZeroMemory(telemetry, sizeof(*telemetry));
    return FALSE;
}

static const UBM_EXTERNAL_ADAPTER g_templateAdapter =
{
    sizeof(UBM_EXTERNAL_ADAPTER),
    UBM_EXTERNAL_ADAPTER_ABI,
    L"Universal Battery Manager adapter template",
    TemplateCanHandle,
    TemplateQuery
};

__declspec(dllexport)
const UBM_EXTERNAL_ADAPTER* WINAPI UBM_GetAdapterPlugin(DWORD hostAbiVersion)
{
    if (hostAbiVersion != UBM_EXTERNAL_ADAPTER_ABI)
        return NULL;
    return &g_templateAdapter;
}
