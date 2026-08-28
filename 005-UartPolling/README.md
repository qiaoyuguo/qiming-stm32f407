# 005-UartPolling (UART 轮询收发)

A **UART polling echo** example for the STM32F407ZGTx. UART5 is driven entirely in
**polling (blocking) mode** — no interrupts, no DMA: the firmware prints a banner,
then blocks in `HAL_UARTEx_ReceiveToIdle()` waiting for a line of input, and echoes
it back framed by newlines and a `> ` prompt, so it is always obvious on the
terminal that the text you see was **sent back by the board**, not left over from
your own typing.

## Hardware Requirements

| Resource | Pin | Role |
| --- | --- | --- |
| UART5_TX | PC12 (AF8) | Transmit, push-pull alternate function |
| UART5_RX | PD2 (AF8) | Receive, alternate function |
| USB-TTL adapter | 3.3 V level | TX→adapter RX, RX→adapter TX, GND common |
| Debug probe | DAP-Link via SWD (CMSIS-DAP) | — |

UART5 is configured as **115200 baud, 8 data bits, 1 stop bit, no parity, no
hardware flow control** (`UART_MODE_TX_RX`, `UART_HWCONTROL_NONE`). GPIOs are set
to `GPIO_SPEED_FREQ_VERY_HIGH` with no internal pull. UART5 hangs on **APB1
(42 MHz)**, which is its default kernel clock.

## Features

- System clock configured to **168 MHz** (HSE **8 MHz** → PLL: M=8, N=336, P=2).
  Note: earlier projects in this workspace assumed a 25 MHz HSE; the schematic of
  this board shows an **8 MHz** crystal (Y2), so `PLLM = 8` here — otherwise the
  real baud rate would be off by 3.125× and everything would look like garbage.
- **Polling-only UART driver** — `HAL_UART_Transmit()` / `HAL_UARTEx_ReceiveToIdle()`
  with `HAL_MAX_DELAY`; the CPU simply spins on UART status flags.
- **Idle-line framing** — `HAL_UARTEx_ReceiveToIdle()` returns when the RX line has
  been idle for one character time, giving a natural "receive one line" semantic
  without knowing the message length in advance (buffer capped at 100 bytes).
- **Unambiguous echo framing** — every echo is wrapped as `\r\n` + data + `\r\n> `,
  so each board reply starts on a fresh line followed by a new prompt.
- Power-on banner `=== UART5 Polling Echo Ready ===` proves the TX path without
  requiring any input.

## How It Works

1. **Startup** — `HAL_Init()`, `SystemClock_Config()` (168 MHz), `MX_GPIO_Init()`,
   `MX_TIM3_Init()`, `MX_UART5_Init()`, then the banner is transmitted.

2. **UART5 configuration (`usart.c`)**:
   ```c
   huart5.Instance         = UART5;
   huart5.Init.BaudRate    = 115200;
   huart5.Init.WordLength  = UART_WORDLENGTH_8B;
   huart5.Init.StopBits    = UART_STOPBITS_1;
   huart5.Init.Parity      = UART_PARITY_NONE;
   huart5.Init.Mode        = UART_MODE_TX_RX;
   huart5.Init.HwFlowCtl   = UART_HWCONTROL_NONE;
   huart5.Init.OverSampling = UART_OVERSAMPLING_16;
   ```

3. **Main loop (`main.c`)** — one blocking receive per iteration, then echo:
   ```c
   if (HAL_OK == HAL_UARTEx_ReceiveToIdle(&huart5, rx_buf, 100, &rx_size, HAL_MAX_DELAY))
   {
     HAL_UART_Transmit(&huart5, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
     HAL_UART_Transmit(&huart5, rx_buf, rx_size, HAL_MAX_DELAY);
     HAL_UART_Transmit(&huart5, (uint8_t *)"\r\n> ", 4, HAL_MAX_DELAY);
   }
   ```

## Using Tabby as the Serial Terminal

This project was tested with [**Tabby**](https://tabby.sh/) (Terminal) and its
**Serial** plugin. Recommended profile:

| Tabby setting | Value | Why |
| --- | --- | --- |
| Baud rate | **115200** | Must match `huart5.Init.BaudRate` |
| Data bits | 8 | `UART_WORDLENGTH_8B` |
| Stop bits | 1 | `UART_STOPBITS_1` |
| Parity | None | `UART_PARITY_NONE` |
| Flow control (RTS/CTS, XON/XOFF) | **None** | `UART_HWCONTROL_NONE` — if you enable flow control here the board never sends because it cannot assert CTS |
| **Local echo** | **Off** | The firmware echoes; with local echo on, every character appears **twice** |
| Enter key sends | **CR (`\r`)** or CR+LF | Anything works — the firmware re-frames output with `\r\n` itself |

Expected session:

```
=== UART5 Polling Echo Ready ===
>
hello                      ← typed by you

hello                      ← echoed by the board (fresh line + prompt proves it)
>
```

Things worth knowing about Tabby (and serial terminals in general):

- **No auto echo** — Tabby does *not* display what you type in the output area by
  default; the characters you see while typing live in the **input box** and are
  cleared when you press Enter. What appears in the output area comes only from
  the board. This is why the echo framing above matters: the `\r\n` + `> ` prompt
  makes each board reply visually unmistakable.
- **The input box is not the output area** — "my typed text disappeared after
  Enter" is normal; look *above* for the board's echo.

## Implementation Notes

- **Polling vs. interrupt/DMA** — the simplest possible UART model: easy to debug,
  zero latency for one byte, but the CPU blocks for the whole transfer. Fine for a
  debug console; for real applications move to interrupt or DMA mode.
- **Why `ReceiveToIdle` and not `HAL_UART_Receive(1 byte)`** — idle-line detection
  groups whatever the peer sends into one "frame" without a known length. Caveat:
  at human typing speed every keystroke is already "idle" (a character lasts ~87 µs
  at 115200), so the function returns once per keystroke — which is exactly the
  per-line behavior wanted for an echo console here.
- **Baud rate depends on the clock tree** — the BRR divider is computed from PCLK1
  (42 MHz). If the real crystal frequency differs from `HSE_VALUE`, the actual baud
  rate shifts proportionally and the terminal shows mojibake. This bit us once:
  the board has an 8 MHz crystal but the project originally assumed 25 MHz.
- **Generated code** — the project skeleton (startup file, linker script, HAL
  drivers, `usart.c`, `tim.c`, clock config) is generated by **STM32CubeMX** and
  kept under the `USER CODE` sections, so it can be safely regenerated.

## Build & Flash

Build (Debug, default):

```sh
cmake --preset Debug && cmake --build --preset Debug
```

The resulting firmware is at `build/Debug/005-UartPolling.elf`.

To flash and run, press **F5** in VS Code (`cortex-debug` + OpenOCD) — it builds,
flashes via the DAP-Link probe and halts at `main`. You can also flash manually:

```sh
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
        -c "program build/Debug/005-UartPolling.elf verify reset exit"
```

## Project Layout (relevant files)

```
Core/
├── Inc/
│   ├── main.h
│   ├── usart.h             # huart5 handle declaration
│   ├── tim.h
│   └── stm32f4xx_it.h
└── Src/
    ├── main.c              # Entry point: banner + echo loop
    ├── usart.c             # UART5 115200-8N1 init + PC12/PD2 AF8 GPIO config
    ├── tim.c               # TIM3 (unused by the UART path, kept from CubeMX)
    └── system_stm32f4xx.c  # System clock
```

## License

HAL and driver code is from STMicroelectronics (STM32F4xx HAL, CMSIS). See
`Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`.
