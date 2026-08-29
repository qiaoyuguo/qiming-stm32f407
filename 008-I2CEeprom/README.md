# 008-I2CEeprom — STM32F407 Hardware I2C with AT24C02 EEPROM

Driver for the onboard AT24C02 EEPROM on the Qiming high-spec STM32F407 board,
using hardware I2C1, with test results printed over UART5 via `printf`.

## 1. Hardware

| Signal | MCU Pin | Notes |
|--------|---------|-------|
| I2C1_SCL | **PB8** | AF4, open-drain |
| I2C1_SDA | **PB9** | AF4, open-drain |
| UART5 TX | — | printf debug output |

- The onboard EEPROM has pull-ups and address pins strapped (A2/A1/A0 = GND);
  7-bit device address is `0x50`.
- STM32F4 I2C1 has **two** pin groups: PB6/PB7 and **PB8/PB9**. This board uses
  PB8/PB9.

## 2. Software Design

### 2.1 File Structure

```
Core/Inc/eeprom.h    driver interface
Core/Src/eeprom.c    driver implementation (no CubeMX-generated code touched)
Core/Src/main.c      test sequence + __io_putchar printf redirect
CMakeLists.txt       user sources registered in target_sources
```

### 2.2 Key Design Points

**Address format**
HAL expects the 7-bit address shifted left by one into the `DevAddress`
parameter:

```c
#define EEPROM_DEV_ADDR  (0x50 << 1)   /* = 0xA0; R/W bit filled in by hardware */
```

There is no need (and no way) to pass 0xA0/0xA1 manually.

**Page write limitation**
The AT24C02 has an 8-byte page; a page write must stay within one page.
`EEPROM_Write` splits the buffer using:

```
chunk = min(remaining, PAGE_SIZE - addr % PAGE_SIZE)
```

Reads have no page restriction and can be arbitrarily long.

**Write-cycle wait**
After each page write the chip is busy for up to 10 ms and will not ACK.
A fixed `HAL_Delay(10)` is used (rationale in §3.3).

**API layers**

| Layer | Functions |
|-------|-----------|
| Byte | `EEPROM_WriteByte` / `EEPROM_ReadByte` |
| Buffer | `EEPROM_Write` (page-aware) / `EEPROM_Read` |
| Block | `EEPROM_Fill` / `EEPROM_EraseAll` / `EEPROM_Verify` |
| Utility | `EEPROM_IsReady` / `EEPROM_CRC16` / `EEPROM_Dump` / `EEPROM_PageOf` / `EEPROM_BytesToPageEnd` |

All interfaces guard against out-of-range addresses (>= 256) and NULL pointers.

### 2.3 Test Sequence (main.c, simple to complex)

1. Single-byte write/read (basic bus check)
2. 4-byte same-page write/read
3. `Fill` 0x5A → `Verify`
4. Write test pattern, cross-check EEPROM-side CRC16 against a RAM-side
   software CRC
5. `Dump` hex dump for visual inspection
6. Cross-page 10-byte write + Verify
7. `EraseAll` → verify all 0xFF (**last**, so it cannot corrupt earlier test data)

Actual serial output (2026-08-29):

```
Step1 (1 byte)       : PASS (W=0xA5 R=0xA5)
Step2 (4 byte)       : PASS
Step3.1 Fill 0x5A @0x20 len=16 : PASS
Step3.2 Verify 0x5A @0x20      : PASS
Step4   CRC16 (eeprom=0xD77D soft=0xD77D) : PASS
Step5   Dump first 4 pages:
0000: 10 21 32 43 54 65 76 87 98 A9 FF FF FF FF FF FF  .!2CTev.........
0010: 11 22 33 44 FF FF FF FF FF FF FF FF FF FF FF FF  ."3D............
Step6   Cross-page 10B w+verify : PASS
Step7.1 EraseAll               : PASS
Step7.2 Verify 0xFF @0x00      : PASS
```

Output notes:
- In `Step4` both the EEPROM-side and RAM-side CRC compute `0xD77D`, mutually
  confirming the implementation.
- In the `Step5` dump, row `0000` holds the 10-byte test pattern; row `0010`
  starts with `11 22 33 44` written in Step2; `FF` bytes are untouched cells.
- `Step7` confirms the whole chip can be reliably erased.
- Re-running after a power cycle still passes Steps 1/2, proving data retention.

## 3. Troubleshooting Guide

### 3.1 CubeMX assigns I2C1 to PB6/PB7 but the board uses PB8/PB9

**Symptom**: CubeMX enabled I2C1 and automatically picked PB6/PB7, but the
schematic shows the EEPROM on PB8/PB9. The device never ACKs.

**Solutions (either works)**:

*Option A — reassign in CubeMX (recommended, keeps .ioc consistent)*
1. Open the `.ioc`, in the pinout view right-click PB6 and PB7 →
   `GPIO_Mode` → `Reset_State` to free them.
2. Click **PB8** → select `I2C1_SCL`, click **PB9** → select `I2C1_SDA`
   (both are in the AF4 I2C1 alternate function set).
3. `Project → Generate Code`. Verify the generated `i2c.c` now shows
   `PB8 ------> I2C1_SCL` and `PB9 ------> I2C1_SDA`.

*Option B — patch the generated code directly*
Edit `HAL_I2C_MspInit()` / `HAL_I2C_MspDeInit()` in `Core/Src/i2c.c`:
replace `GPIO_PIN_6|GPIO_PIN_7` with `GPIO_PIN_8|GPIO_PIN_9` (and the comment).
Downside: the change is inside the auto-generated section and will be
**overwritten on the next CubeMX regeneration** — this is why Option A is
preferred. Only the `/* USER CODE BEGIN/END */` sections survive regeneration.

**General rule**: peripherals with multiple pin groups (I2C1, USART, SPI…) must
always be cross-checked against the board schematic, because CubeMX has no
knowledge of which group the board actually routed.

### 3.2 Device never ACKs — systematic checklist

1. **Bus scan**: probe every address 0x08–0x77 with
   `HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 2, 10)`. It distinguishes
   "wrong pins / no device" (nothing ACKs) from "wrong EEPROM address"
   (something ACKs at 0x51–0x57 instead of 0x50).
2. Check address straps: A2/A1/A0 grounded → 0x50; any pulled high shifts it.
3. Check pull-ups on SCL/SDA (onboard EEPROMs usually already have them).
4. Check a WP (write-protect) pin if present — must be low to write.

### 3.3 Read/write direction: do NOT use 0xA0/0xA1 manually

The doubt "reads use 0xA1, writes use 0xA0 — which do I pass?" is resolved by
the HAL convention: pass the 7-bit address shifted left by one (0xA0); the R/W
direction bit is inserted by the I2C peripheral automatically. Manually passing
0xA1/0xA0 breaks the parameter contract.

> **Lesson**: confirm a library function's parameter conventions (7-bit vs
> 8-bit address) before use; do not carry over bare-register habits.

### 3.4 Multi-byte writes fail intermittently (ErrorCode=0, state stuck)

**Symptom**: single-byte r/w passed, multi-byte writes sometimes failed with
`hi2c1.ErrorCode == 0` and the HAL state stuck non-READY (`state=32`).

**Root cause**: the code polled `HAL_I2C_IsDeviceReady` after each page write
to detect end-of-write-cycle. The AT24C02 write cycle is 5 ms *typical* but
**10 ms maximum**; the poll occasionally timed out and misreported. This HAL
version also fails to restore the handle state after `IsDeviceReady`.

**Fix**: replaced polling with a fixed `HAL_Delay(EEPROM_WRITE_CYCLE_MS=10)` —
deterministic, at the cost of a few ms per page write.

**Diagnosis technique that found it**:
- Print `hi2c1.ErrorCode` and `HAL_I2C_GetState()` in every failure branch —
  `ErrorCode=0` with a stuck state pointed at the polling helper, not the bus.
- Incremental testing (1 byte → 4 bytes → cross-page) isolated the failure to
  the multi-byte path immediately.

## 4. Lessons Learned

1. **Verify pin assignments against the schematic** whenever a peripheral has
   multiple pin groups; never trust the auto-assigned pins blindly.
2. **Design waits around the datasheet maximum** (10 ms), not the typical
   value (5 ms). Polling with a too-short timeout produces flaky failures that
   are hard to reproduce.
3. **Step-by-step validation**: pass the simplest case first, then add
   complexity — this localizes bugs to the smallest scenario.
4. **Observability**: every failure branch should print distinct diagnostics
   (HAL return value, ErrorCode, State).
5. **Cross-check results**: e.g. compute CRC on both the EEPROM side and the
   RAM side instead of trusting a hard-coded expected value.
6. **Destructive operations last**: put `EraseAll` at the end of the test
   sequence so re-runs never pollute earlier results.

## 5. Build & Debug

```bash
cmake --preset Debug && cmake --build --preset Debug
```

- VS Code F5 uses `.vscode/launch.json` (cortex-debug + OpenOCD + DAP-Link)
- View printf output on UART5 (same configuration as the 007-UartPrintf project)
