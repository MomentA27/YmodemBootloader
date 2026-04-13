# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build Debug (default)
cmake --preset Debug
cmake --build --preset Debug

# Build Release
cmake --preset Release
cmake --build --preset Release

# Clean build
cmake --build --preset Debug --target clean
```

Build artifacts go to `build/{Debug|Release}/`.

## Hardware

- MCU: STM32F407ZGT6 (LQFP144)
- External Flash: NM25Q128 (SPI, 16MB)
- EEPROM: AT24C02 (I2C)
- UART: USART1 (for Ymodem communication)
- Debug: SWD on PA13/PA14

## Memory Layout

- FLASH: 0x08000000 (1MB total)
- Bootloader size: 32KB (0x8000)
- Application entry: 0x08008000
- RAM: 0x20000000 (192KB)
- CCMRAM: 0x10000000 (64KB)

## Architecture

```
Core/Src/           - STM32CubeMX generated (main.c, gpio.c, dma.c, spi.c, usart.c)
01_BSP/             - Hardware drivers
  AT24C02/          - EEPROM driver (OTA state storage)
  NM25Q128/         - External SPI flash driver (firmware storage)
  UARTPRO/          - UART protocol layer
  INFLASH/          - Internal flash operations
02_Libraries/       - Reusable libraries
  02_1-Common/      - Ymodem, IIC_BUS, DWT_delay
  02_2-ThirdParty/  - elog, SEGGER RTT
03_adapter/         - High-level adapters
  03_1-jumptoapp/   - App jump and OTA state management
  03_X-Debug/       - Debug utilities
```

## Key Components

**Ymodem Receive** (`02_Libraries/02_1-Common/Ymodem/`): Firmware download protocol. `Ymodem_Receive(base_addr, max_size)` writes received firmware directly to flash.

**OTA State** (`03_adapter/03_1-jumptoapp/`): Stored in AT24C02 EEPROM at addresses 0x00-0x08. States: NO_APP_UPDATE (0x00), APP_DOWNLOADING (0x11), APP_DOWNLOAD_COMPLETE (0x22).

**Jump to App**: Checks EEPROM OTA state. If APP_DOWNLOAD_COMPLETE, jumps to 0x08008000.

## Build System

- CMake with Ninja generator
- STM32CubeMX CMake integration in `cmake/stm32cubemx/`
- Three library targets: `bsp`, `libraries`, `adapter`
- CMSIS DSP and NN libraries included from Drivers/
