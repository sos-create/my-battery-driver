# Universal Battery Manager

Windows battery diagnostics and power-plan utility.

## Build outputs
The workflow builds CLI and native Win32 GUI executables for Win32 and x64. Download `UniversalBatteryManager-Win32` or `UniversalBatteryManager-x64` from a successful Actions run.

## GUI
Run `UniversalBatteryManagerGUI.exe`. It shows Windows aggregate battery status, battery-class PnP devices, individual telemetry when exposed by Windows Battery Class, and the active Windows power plan. Refresh is automatic every five seconds. Power buttons request the matching built-in Windows power scheme; Windows may reject unavailable schemes.

## Console commands
- `UniversalBatteryManager.exe --help`
- `UniversalBatteryManager.exe --list-plans`
- `UniversalBatteryManager.exe --set-plan balanced`
- `UniversalBatteryManager.exe --set-plan performance`
- `UniversalBatteryManager.exe --set-plan saver`

## Adapter support
Windows Battery Class supports read-only battery telemetry. ACPI Control Method Battery is discovered through PnP and read through Windows Battery Class. SMBus, I2C and USB/HID are identified for diagnostics only; vendor-specific protocols are required before direct communication. The adapter catalog is an extension point, not a claim of universal compatibility.

## Safety and limitations
No battery-controller registers are written. Temperature, cycle count, voltage, capacity and charge state depend on hardware and Windows driver support. Generic software cannot safely control proprietary controllers without protocol documentation or a verified adapter. Windows power-plan changes do not change proprietary battery charge thresholds.

## External adapter modules

The GUI looks for adapter DLLs in an `Adapters` directory beside the executable. A module must export `UBM_GetAdapterPlugin` and return a descriptor using the versioned contract in `BatteryAdapterSdk.h`. The host validates the ABI version and required callbacks before accepting the module. An adapter can identify matching hardware and return telemetry fields supported by its device.

Only install DLLs from sources you trust: loading an adapter executes its code inside the application. The current adapter contract is read-only; it does not authorize arbitrary register writes or Windows kernel-driver installation. No external adapters are bundled by default.
