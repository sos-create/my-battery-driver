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
