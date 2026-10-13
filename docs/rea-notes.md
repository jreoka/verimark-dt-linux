# Hardware profile: Kensington VeriMark DT (from the Windows driver)

Reverse engineered with REA from
`Kensington-132 _v6_0_9_1132_x64_inf_WHQL_pass` (driver 6.0.9.1132,
2020-12-15). Purpose: prove this exact unit is the device the Linux
driver in this repo targets, so there is no VID/PID guessing.

## Identity

- USB IDs (from `synaWudfBioUsb.inf`, all three OS sections):
  `USB\VID_06CB&PID_00DE&MI_01` (Synaptics reference) and
  **`USB\VID_047D&PID_00F2&MI_01`** (Kensington VeriMark DT).
- Installed name: "VeriMark DT Fingerprint Key", biometric class
  `{53D29EF7-377C-4D14-864B-EB3A85769359}`.
- Internal family "132" (`SynaCP132.dll`, `synaFpAdapter132.dll`,
  `WBFResetService132.exe`, `synaWudfBioUsb132.dll`).
- The fingerprint function lives on **USB interface 1** (`MI_01`).
  Interface 0 is the HID/FIDO2 function — leave it on `usbhid`.

## Sensor family

- **Synaptics Tudor**: the WBDI driver exports
  `tudorCmdFormat / tudorCmdDeleteObject / tudorCmdCleanup /
  tudorCmdMisEnrollCommit`, a "Tudor Storage Module" with a
  `USER_PARTITION`, and states "Only tlsSecurityTypeEcc and
  tlsSecurityTypePsk are supported in Tudor family sensors".
- Storage/Engine/Sensor adapters are one binary
  (`synaFpAdapter132.dll` → `WbioQuery{Sensor,Engine,Storage}Interface`).
- Templates live in sensor flash (`RetrieveTemplatesFromFlash` with a
  host-side cache); enrollment is Match-in-Sensor.

## USB transport model (for driver authors)

- WinUSB (`WINUSB.DLL`, `WinUsb_*` imports): a request bulk endpoint, a
  reply bulk endpoint, and an interrupt endpoint
  (`Setting requestBulkEndpoint / replyBulkEndpoint / interruptEndpoint`,
  `Failed to read data from interrupt EP`, `WinUsb_ControlTransfer`).
- `CBiometricDeviceUSB::WBFUsb{Initialize,ResetDevice,ResetPipe,AbortPipe,
  GetPipePolicy,SetPowerPolicy,QueryInterfaceSettings}` — endpoint
  numbers are queried at runtime, not constants.
- Crypto: ECDH/ECDSA session + AES-GCM via BCrypt
  (`Could not create secret agreement`, `Failed to do AES-GCM`),
  PSK identity (`Failed to get SSI_STORAGE_PSK_ID`), TLS-secured sensor
  sessions (`Sensor is in secure session but host is not`).
- Control surface is `SSI_IOCTL_CODE_*` / `VCSDRV_IOCTL_*`
  (`PARTITION_FORMAT`, `DEVICE_EVENT_LISTEN`, ...).

## Reset/erase path (see also sibling repo `kensington-reset`)

- Whole-database erase: `WinBioControlUnitPrivileged`, storage
  component, `CTL_CODE(0x44, 2058)` = `0x442028`.
- Device restart: `DIF_PROPERTYCHANGE` restart of matching HWIDs, or the
  `Global\SynaDevResetEvent-132` → `...ConfirmEvent-132` service events.
