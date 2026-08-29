# 007-UartPrintf (UART printf 重定向)

A **`printf()` retargeting** example for the STM32F407ZGTx — building on the UART
setup of [005-UartPolling](../005-UartPolling/README.md) and
[006-UartInterrupt](../006-UartInterrupt/README.md). Instead of calling
`HAL_UART_Transmit()` directly everywhere, the C library's `printf()` output is
redirected to UART5 by overriding the low-level **`__io_putchar()`** hook: any
`printf` in the codebase then streams out of the serial port at 115200 baud,
turning the board into a plain-text debug console.

## Hardware Requirements

| Resource | Pin | Role |
| --- | --- | --- |
| UART5_TX | PC12 (AF8) | Transmit, push-pull alternate function |
| UART5_RX | PD2 (AF8) | Receive, alternate function |
| USB-TTL adapter | 3.3 V level | TX→adapter RX, RX→adapter TX, GND common |
| Debug probe | DAP-Link via SWD (CMSIS-DAP) | — |

UART5 is configured as **115200 baud, 8 data bits, 1 stop bit, no parity, no
hardware flow control** (`UART_MODE_TX_RX`, `UART_HWCONTROL_NONE`), hanging on
**APB1 (42 MHz)** — identical to 005/006, so the same wiring and the same
serial-terminal profile work for all three.

## Features

- System clock configured to **168 MHz** (HSE **8 MHz** → PLL: M=8, N=336, P=2;
  `HSE_VALUE = 8000000` in `stm32f4xx_hal_conf.h`, matching the board's Y2 crystal).
- **`printf()` retargeting via `__io_putchar()`** — the newlib/syscalls layer
  (`syscalls.c`) forwards every character of `printf` output to `__io_putchar()`;
  overriding that single function retargets the whole standard library.
- **Character-by-character blocking TX** — each byte is sent with
  `HAL_UART_Transmit(&huart5, ..., 1, HAL_MAX_DELAY)`, the simplest correct
  retarget (no ring buffer, no DMA, no interrupts needed).
- **Standard formatted output** — `%d`, `%u`, `%x`, `%s`, `%f` etc. all work
  through the normal C library, so debug traces look exactly like desktop code.
- Power-on banner `Uart printf example\r\n` proves the retarget works end to end.
- Main loop stays **empty** — the example demonstrates TX only; combine with
  006's `ReceiveToIdle_IT` for a full interactive console.

## How It Works

1. **Startup** — `HAL_Init()`, `SystemClock_Config()` (168 MHz), `MX_GPIO_Init()`,
   `MX_TIM3_Init()`, `MX_UART5_Init()`, then the first `printf` goes out:
   ```c
   MX_GPIO_Init();
   MX_TIM3_Init();
   MX_UART5_Init();
   printf("Uart printf example\r\n");
   ```

2. **Retarget hook (main.c)** — the one function that routes `printf` to UART5:
   ```c
   int __io_putchar(int ch)
   {
     HAL_UART_Transmit(&huart5, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
     return ch;
   }
   ```

3. **UART5 configuration (`usart.c`)**:
   ```c
   huart5.Instance          = UART5;
   huart5.Init.BaudRate     = 115200;
   huart5.Init.WordLength   = UART_WORDLENGTH_8B;
   huart5.Init.StopBits     = UART_STOPBITS_1;
   huart5.Init.Parity       = UART_PARITY_NONE;
   huart5.Init.Mode         = UART_MODE_TX_RX;
   huart5.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
   huart5.Init.OverSampling = UART_OVERSAMPLING_16;
   ```

4. **Output chain** — how a `printf` call reaches the wire:
   ```text
   printf("...")
     → newlib _write(file, ptr, len)      (syscalls.c, weak override)
       → __io_putchar(*ptr++) per char    (main.c, USER CODE)
         → HAL_UART_Transmit(&huart5, &ch, 1, HAL_MAX_DELAY)
           → UART5 TX (PC12)
   ```

## Direct HAL (005/006) vs. printf (007)

| Aspect | 005/006-UartPolling / Interrupt | 007-UartPrintf |
| --- | --- | --- |
| Output API | `HAL_UART_Transmit()` per message | `printf()` with format strings |
| Formatting | Manual (`sprintf` into a buffer) | Built-in (`%d`, `%x`, `%f`, …) |
| Retarget hook | Not needed | `__io_putchar()` override |
| Code size | Smaller (no stdio) | Larger (newlib pulls in printf) |
| RX | Polling (005) / idle interrupt (006) | Not covered (TX only) |
| Best suited for | Learning UART mechanics | Everyday debug tracing |

Transmit remains **blocking** (`HAL_UART_Transmit` with `HAL_MAX_DELAY`) —
fine for debug prints; a next step would be a TX ring buffer with
`HAL_UART_Transmit_IT`/DMA to avoid stalling `printf` callers.

## Serial Terminal Settings

Tested with [**Tabby**](https://tabby.sh/) (Serial plugin) — same profile as 005/006:

| Setting | Value |
| --- | --- |
| Baud rate | **115200** |
| Data bits / Stop bits / Parity | 8 / 1 / None |
| Flow control | **None** (RTS/CTS and XON/XOFF off) |

Expected session (no input required — output only):

```
Uart printf example
```

## Implementation Notes

- **`__io_putchar` is the standard ARM/newlib retarget point** — the
  CubeMX-generated `syscalls.c` declares `extern int __io_putchar(int)`
  (weak) and its `_write()` loops over the buffer calling it. Overriding it in
  `main.c` (strong symbol) is all that's needed; no linker flags or vendor
  retarget libraries are involved.
- **Line endings** — the banner uses `\r\n` explicitly; terminals expect CRLF,
  and `printf("...\n")` alone would show staircase output.
- **Blocking TX latency** — at 115200 baud each byte takes ~87 µs, so a long
  `printf` from a tight loop (or from an ISR!) will slow the caller down; keep
  prints out of interrupt context or move to interrupt/DMA TX later.
- **Code-size cost** — linking `printf` pulls in the newlib formatting code;
  if flash gets tight, `-u _printf_float` removal / `nano.specs` can shrink it.
- **TIM3 is initialized but unused** — `MX_TIM3_Init()` is kept from the CubeMX
  configuration (inherited from earlier projects) but its interrupt is never
  started; it plays no role in the UART path.
