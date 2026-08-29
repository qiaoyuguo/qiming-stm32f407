# 009-SpiW25Q128 — STM32F407 + W25Q128 SPI Flash Driver

Bare-metal STM32F407ZGTx project (STM32CubeMX + CMake + arm-none-eabi-gcc) that
implements a full W25Q128 (16MB SPI NOR Flash) driver over SPI1 with software
chip-select, plus a serial test suite to verify every driver API.

All 11 API verification tests pass:

```
[TEST0 readID] pass: JEDEC ID = 0xEF4018
[ERASE] ok
[TEST1 32B@0x10] pass
[TEST2 3pages@0x80] pass
[TEST3 1sector@0x800] pass
[TEST4 erase&checkFF@0x1000] pass
[TEST5 cross-sector@0xFFE] pass
[TEST6 writedisable] pass
[TEST7 status2] pass: SR2 = 0x00
[TEST8 uniqueid] pass: D2666C741B380D2C
[TEST9 powerdown] pass
SR1 = 0x1C
[TEST10 blocklock] pass
==== result: 11 pass, 0 fail ====
```

---

## 1. Hardware Configuration

### 1.1 MCU & Peripherals

| Item | Value |
|---|---|
| MCU | STM32F407ZGTx, LQFP144 |
| System clock | HSE 8MHz → PLL ×42 → 168MHz (APB2 = 84MHz) |
| Debug | DAP-Link + OpenOCD (SWD) |
| Log output | UART5 (`printf` via `__io_putchar`) |
| Flash | W25Q128JV (16MB SPI NOR) |

### 1.2 SPI1 Pin Mapping (this project)

| Signal | Pin | AF |
|---|---|---|
| SPI1_SCK | PB3 | AF5 |
| SPI1_MISO | PB4 | AF5 |
| SPI1_MOSI | PB5 | AF5 |
| CS (software) | PG8 (`W25Q128_CS_Pin`) | GPIO output, idle high |

SPI1 is configured with `HAL_SPI_Transmit/Receive` in blocking mode and
`NSS = SPI_NSS_SOFT`; the CS pin is toggled manually in the driver.

### 1.3 W25Q128 Wiring

| W25Q128 pin | Connect to |
|---|---|
| VCC / GND | 3.3V / GND (add 100nF decoupling cap) |
| /CS | PG8 |
| CLK | PB3 |
| DI (MOSI) | PB5 |
| DO (MISO) | PB4 |
| /WP | 3.3V (never drive low unless needed) |
| /HOLD | 3.3V |

### 1.4 SPI1 Alternate Pin Combinations

SPI1 on STM32F407 can be mapped to **three** sets of pins. If your board routes
the flash differently, change the pin mux in `Core/Src/spi.c`
(`HAL_SPI_MspInit()`), regenerate from the `.ioc`, or edit `009-SpiW25Q128.ioc`
in CubeMX directly.

| Option | SCK | MISO | MOSI | Notes |
|---|---|---|---|---|
| A | PB3 | PB4 | PB5 | AF5, after remap (used here) |
| B | PA5 | PA6 | PA7 | AF5, default (often shared with other shields) |
| C | PA5 | PB4 | PB5 | AF5, mixed |

**How to adjust:**

1. Open `009-SpiW25Q128.ioc` in STM32CubeMX → Pinout view → click the target
   pin → select `SPI1_SCK / SPI1_MISO / SPI1_MOSI` → regenerate code, **or**
2. Edit `HAL_SPI_MspInit()` in `Core/Src/spi.c` manually. Example for Option B
   (PA5/PA6/PA7):

```c
__HAL_RCC_GPIOA_CLK_ENABLE();
GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull      = GPIO_NOPULL;
GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

(Keep the `DeInit` function consistent when editing manually.)

CS pin choice is free — any free GPIO works. To change it, update
`W25Q128_CS_Pin` / `W25Q128_CS_GPIO_Port` in `Core/Inc/main.h` and re-run
`MX_GPIO_Init()` accordingly.

### 1.5 UART5 (log console)

UART5 is used for `printf` output: check `Core/Src/usart.c` for the exact
TX pin; connect a USB-TTL adapter (3.3V) and open a terminal at the configured
baud rate (see `huart5.Init.BaudRate`, default in this project 115200-8-N-1).

---

## 2. Build & Debug

```bash
# configure + build (Debug)
cmake --preset Debug && cmake --build --preset Debug

# release
cmake --preset Release && cmake --build --preset Release
```

Artifacts: `build/Debug/009-SpiW25Q128.elf`

VS Code:

- **Build**: task `Compile Debug` (Ctrl+Shift+B)
- **Flash & debug**: launch config `STM32F407 + DAP-Link (cortex-debug)`
  (requires `cortex-debug` extension, `openocd` at `/opt/homebrew/bin/openocd`)
- Serial log: UART5 at 115200 baud

---

## 3. W25Q128 Memory Organization

```
16MB total
 ├── 512 x 64KB Block          <- Block Erase (0xD8), 32KB variant (0x52)
 │     ├── 16 x 4KB Sector     <- Sector Erase (0x20), smallest erase unit
 │     │     └── 16 x 256B Page  <- Page Program (0x02), programming unit
```

- **Program** can only clear bits (1 → 0). Setting bits back to 1 requires an
  erase (whole sector at minimum).
- **Read** has no alignment/size restrictions.
- Page Program must not cross a page boundary (low 8 address bits wrap).

## 4. Driver Design

Files: `Core/Inc/w25q128.h`, `Core/Src/w25q128.c` (added to
`target_sources` in `CMakeLists.txt`).

### Layering

```
Public API
  W25Q128_Init / ReadID / Read / Write / Erase* / WaitBusy
  W25Q128_ReadStatus / ReadStatus2 / WriteStatusReg
  W25Q128_WriteEnable / WriteDisable
  W25Q128_ReadUniqueID / PowerDown / ReleasePowerDown
  W25Q128_LockBlock / UnlockBlock / ReadBlockLock
        │
Command layer   SPI_SendCmdAddr (cmd + 24-bit addr, MSB first), ReadWithCmd
        │
SPI layer       HAL_SPI_Transmit / Receive (blocking, 500ms timeout)
        │
GPIO            CS_Low / CS_High  (PG8)
```

### Key behaviors

- `W25Q128_Write(addr, buf, len)` is a **high-level write helper**: it splits
  any length/alignment into multiple Page Program operations limited by the
  page boundary (`chunk = 256 - addr % 256`), re-issues Write Enable before
  each program, and waits for BUSY after each.
- `W25Q128_WaitBusy(timeout)` polls SR1 BUSY with `HAL_GetTick()`-based
  timeout; sector erase waits up to 2s, chip erase up to 200s.
- Every erase/program op: `WriteEnable → cmd(+addr) → WaitBusy`.
- `W25Q128_Init()` reads the JEDEC ID and returns `W25Q128_BAD_ID` on mismatch
  (expected `0xEF4018`).

### Test suite (main.c, USER CODE 2)

| Test | Purpose |
|---|---|
| TEST0 | JEDEC ID == 0xEF4018 |
| ERASE | Erase sectors 0/1 to prepare a clean area |
| TEST1 | 32B write+readback at 0x10 (unaligned, single page) |
| TEST2 | 3 pages (768B) at 0x80 — automatic page-splitting |
| TEST3 | Full 4KB sector write+readback at 0x800 (spans 2 sectors) |
| TEST4 | Erase sector 1, verify all bytes == 0xFF |
| TEST5 | 8B write across sector boundary 0xFFE→0x1002 |
| TEST6 | WREN sets WEL, WRDI clears it |
| TEST7 | Read SR2 |
| TEST8 | Unique ID (0x4B) read twice, stable and not all-FF |
| TEST9 | Power-down → release → JEDEC ID still readable |
| TEST10 | BP-bit global protection: write blocked while SR1=0x1C, allowed after SR1=0x00 |

## 5. Pitfalls Encountered (and how they were fixed)

1. **Writing without re-erasing silently corrupts data**
   TEST5 initially failed because sector 0 had already been programmed by
   TEST1~3; programming can only turn 1→0, so the readback mixed old and new
   bits. Fix: erase the sector before the next write campaign. Rule of thumb:
   *every* write to a previously-written region needs an erase first.

2. **Individual Block Lock (0x36/0x39) not enforced on this chip**
   The lock command set the lock bit (0x3D read back `locked = 1`) but the
   chip still accepted programming. Likely a compatible/clone part where the
   commands are accepted but not implemented. Fix: use **status-register BP
   protection** (`W25Q128_WriteStatusReg(0x1C)` → BP0~BP2 = 1 blocks all
   program/erase) which works on every W25Q/GD25 part. Keep in mind BP
   protection blocks erase as well, and clear it (`0x00`) before any erase.

3. **Page Program must not cross a page boundary**
   Writing 512 bytes starting at address 128 would wrap around and overwrite
   the start of the page if done in one command. The driver therefore splits
   writes at page boundaries automatically (see `W25Q128_Write`).

4. **User code must live inside `USER CODE BEGIN/END` blocks**
   The JEDEC-ID test was first placed outside `USER CODE BEGIN 2` and would
   have been deleted on the next CubeMX code regeneration. All edits to
   generated files (`main.c`, `spi.c`, …) must stay inside USER CODE sections.

5. **`.vscode` launch config must match the ELF name**
   When copying the debug configuration from another project (e.g.
   `008-I2CEeprom`), the `executable` path must be updated to
   `build/Debug/009-SpiW25Q128.elf` or cortex-debug will fail to find the
   firmware.

6. **New driver sources must be added to CMakeLists.txt**
   CubeMX-generated CMake projects do not auto-glob user files:
   add `Core/Src/w25q128.c` to `target_sources` in the top-level
   `CMakeLists.txt`.

7. **`printf` retarget**
   GCC newlib routes `printf` through `__io_putchar`; it is implemented in
   `main.c` using UART5 with `HAL_MAX_DELAY` blocking transmit.

## 6. Possible Next Steps

- Retarget SPI to DMA (`HAL_SPI_Transmit_DMA`) to free the CPU
- Add a parameter-storage layer with magic/length/CRC headers and wear-leveling
- Mount LittleFS over the driver for a POSIX-like file system
- Benchmark read/write/erase throughput with `HAL_GetTick()`
- Test Quad commands (Dual/Quad output read 0x6B/0xEB) if the flash part and
  wiring support QE mode
