# M5Unit-Fingerprint2-Internal-FW

## Overview

### SKU: U203

Internal firmware for the M5Stack Unit Fingerprint2. This repository contains the STM32G031G8Ux firmware projects, the IAP bootloader project, released firmware binaries, and the UART protocol documents used by Unit Fingerprint2.

The firmware works as a bridge and power-management controller for the fingerprint module. It forwards standard fingerprint packets, adds M5Stack control commands for sleep/work mode configuration, module activation, status query, firmware version query, and IAP reset, and manages the fingerprint module power and touch-detection wakeup flow.

## Repository Structure

```text
.
├── code
│   ├── Unit_Fingerprint      # Main application firmware project
│   └── IAP_Fingerprint       # IAP bootloader firmware project
├── firmware
│   └── 2025-09-22-V1         # Released binary, hex, and header firmware images
├── protocol                  # UART protocol documents
├── LICENSE
└── README.md
```

## Firmware Projects

| Project | Path | Description |
| --- | --- | --- |
| Unit_Fingerprint | `code/Unit_Fingerprint` | Main application firmware for Unit Fingerprint2 |
| IAP_Fingerprint | `code/IAP_Fingerprint` | Bootloader/IAP firmware used for firmware update and application jump |

## Hardware & Firmware Notes

- MCU: STM32G031G8Ux
- STM32Cube firmware package: STM32Cube FW_G0 V1.6.2
- Toolchain target: STM32CubeIDE / GCC
- System clock: 64 MHz
- Application address: `0x08002000`
- Host UART: USART2, 115200 baud
- Fingerprint module UART: USART1, 57600 baud, 2 stop bits
- Touch detection: PA8 external interrupt
- Fingerprint module power enable: PC15

## M5Stack Extended Commands

The firmware adds the following M5Stack control commands on top of the fingerprint module protocol:

| Command | Value | Description |
| --- | --- | --- |
| `CMD_SET_SLEEP_TIME` | `0xD0` | Set active sleep timeout |
| `CMD_GET_SLEEP_TIME` | `0xD1` | Read active sleep timeout |
| `CMD_SET_WORK_MODE` | `0xD2` | Set work mode |
| `CMD_GET_WORK_MODE` | `0xD3` | Read work mode |
| `CMD_ACTIVATE_FINGERPRINT_MODULE` | `0xD4` | Wake or activate fingerprint module |
| `CMD_GET_FINGERPRINT_MODULE_STATUS` | `0xD5` | Read fingerprint module status |
| `CMD_SAVE_CONFIGURATION_STATUS` | `0xD6` | Save runtime configuration to flash |
| `CMD_GET_FIRMWARE_VERSION` | `0xD7` | Read STM32 firmware version |
| `CMD_IAP_RESET_STM32_MCU` | `0xFD` | Reset STM32 MCU into IAP flow |

See the protocol documents for packet format and response definitions.

## Released Firmware

Current release files are stored in `firmware/2025-09-22-V1`:

- `U203-Unit-Fingerprint2-STM32-FW-TAO-V1-20250922_0x08000000.bin`
- `U203-Unit-Fingerprint2-STM32-FW-TAO-V1-20250922_0x08000000.hex`
- `bootloader.h`
- `app_with_flash.h`
- `app_without_flash.h`

## Build

1. Open STM32CubeIDE.
2. Import the projects under `code/Unit_Fingerprint` and `code/IAP_Fingerprint`.
3. Select the `Release` configuration.
4. Build the required project.

When updating the main application project, keep the application start address aligned with `0x08002000`, because the IAP bootloader jumps to that address after CRC validation.

## Protocol

- [Unit Fingerprint Protocol EN V1.0](protocol/Unit-Fingerprint-Protocol-EN-V1.0.pdf)
- [Unit Fingerprint Protocol CN V1.0](protocol/Unit-Fingerprint-Protocol-CN-V1.0.pdf)

## Related Link

- [Document & Datasheet](https://docs.m5stack.com/en/unit/Unit_Fingerprint2)
- [Product Page](https://shop.m5stack.com/products/fingerprint-2-unit-a-k323cp)

## License

- [M5Unit-Fingerprint2-Internal-FW - MIT](LICENSE)
