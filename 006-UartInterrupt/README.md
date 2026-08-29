# 006-UartInterrupt (UART 中断收发)

A **UART interrupt-driven echo** example for the STM32F407ZGTx — the interrupt-mode
counterpart of [005-UartPolling](../005-UartPolling/README.md). UART5 reception runs
entirely in the **UART5 global interrupt** with **idle-line (IDLE) detection**:
`HAL_UARTEx_ReceiveToIdle_IT()` arms a 100-byte buffer once at startup, and every
completed frame is echoed from the `HAL_UARTEx_RxEventCallback` — the `while(1)`
main loop stays **completely empty**, so the CPU is free while waiting for input.

## Hardware Requirements

| Resource | Pin | Role |
| --- | --- | --- |
| UART5_TX | PC12 (AF8) | Transmit, push-pull alternate function |
| UART5_RX | PD2 (AF8) | Receive, alternate function |
| USB-TTL adapter | 3.3 V level | TX→adapter RX, RX→adapter TX, GND common |
| Debug probe | DAP-Link via SWD (CMSIS-DAP) | — |

UART5 is configured as **115200 baud, 8 data bits, 1 stop bit, no parity, no
hardware flow control** (`UART_MODE_TX_RX`, `UART_HWCONTROL_NONE`), hanging on
**APB1 (42 MHz)** — identical to 005-UartPolling, so the same wiring and the same
serial-terminal profile work for both.

## Features

- System clock configured to **168 MHz** (HSE **8 MHz** → PLL: M=8, N=336, P=2;
  `HSE_VALUE = 8000000` in `stm32f4xx_hal_conf.h`, matching the board's Y2 crystal).
- **Interrupt reception with idle-line framing** — `HAL_UARTEx_ReceiveToIdle_IT()`
  returns control immediately; the RX interrupt + IDLE line event delimit a "frame"
  without knowing its length in advance (buffer capped at 100 bytes).
- **Non-blocking main loop** — `while(1)` is empty; unlike 005's polling loop the
  CPU does not spin on UART flags and could do other work.
- **Event callback** — the HAL calls `HAL_UARTEx_RxEventCallback(huart, Size)` with
  the actual number of bytes received (either on IDLE or when the buffer is full),
  which echoes the frame and **re-arms** the reception.
- **Unambiguous echo framing** — every reply is wrapped as `\r\n` + data + `\r\n> `,
  so each board reply starts on a fresh line followed by a new prompt.
- Power-on banner `UART5 Receive to Idle Interrupt Test\r\n> ` proves the TX path
  without requiring any input.

## How It Works

1. **Startup** — `HAL_Init()`, `SystemClock_Config()` (168 MHz), `MX_GPIO_Init()`,
   `MX_TIM3_Init()`, `MX_UART5_Init()`, then the banner is transmitted and the
   first reception is armed:
   ```c
   HAL_UART_Transmit(&huart5, (uint8_t *)"UART5 Receive to Idle Interrupt Test\r\n> ", 40, 0xFFFF);
   HAL_UARTEx_ReceiveToIdle_IT(&huart5, rx_buf, 100);
   ```

2. **UART5 configuration (`usart.c`)**:
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
   The **UART5 global interrupt is enabled in the NVIC** (configured in CubeMX,
   `NVIC.UART5_IRQn=true`, preempt priority 0).

3. **Reception flow** — each incoming frame travels through the interrupt chain:
   ```text
   UART5 RX bytes + IDLE event
     → UART5_IRQHandler()            (stm32f4xx_it.c)
       → HAL_UART_IRQHandler(&huart5)
         → HAL_UARTEx_RxEventCallback(&huart5, Size)   (main.c, USER CODE)
             echo: "\r\n" + rx_buf[0..Size) + "\r\n> "
             re-arm: HAL_UARTEx_ReceiveToIdle_IT(huart, rx_buf, 100);
   ```
   ```c
   void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
   {
     if (huart->Instance == UART5)
     {
       HAL_UART_Transmit(huart, (uint8_t *)"\r\n", 2, 0xFFFF);
       HAL_UART_Transmit(huart, rx_buf, Size, 0xFFFF);
       HAL_UART_Transmit(huart, (uint8_t *)"\r\n> ", 4, 0xFFFF);
       HAL_UARTEx_ReceiveToIdle_IT(huart, rx_buf, 100);
     }
   }
   ```
   `Size` is the number of bytes actually received in this frame, so variable-length
   lines are echoed exactly — no trailing garbage from previous frames.

## Polling (005) vs. Interrupt (006)

| Aspect | 005-UartPolling | 006-UartInterrupt |
| --- | --- | --- |
| RX API | `HAL_UARTEx_ReceiveToIdle()` (blocking) | `HAL_UARTEx_ReceiveToIdle_IT()` (IRQ) |
| CPU while waiting | Spins on UART flags in `while(1)` | Free — `while(1)` is empty |
| Frame event | Return of the blocking call | `HAL_UARTEx_RxEventCallback()` |
| NVIC | UART5 IRQ not used | `UART5_IRQn` enabled |
| Where echo lives | Main loop | RX-event callback |
| Best suited for | Simplest possible debug console | Background reception + other work |

Transmit is still **blocking** (`HAL_UART_Transmit` with `0xFFFF` timeout) in both
projects — perfectly fine for short replies; a next step would be
`HAL_UART_Transmit_IT` / DMA, or circular-buffer DMA reception.

## Serial Terminal Settings

Tested with [**Tabby**](https://tabby.sh/) (Serial plugin) — same profile as 005:

| Setting | Value |
| --- | --- |
| Baud rate | **115200** |
| Data bits / Stop bits / Parity | 8 / 1 / None |
| Flow control | **None** (RTS/CTS and XON/XOFF off) |
| Local echo | **Off** (the firmware echoes) |

Expected session:

```
UART5 Receive to Idle Interrupt Test
>
hello                      ← typed by you

hello                      ← echoed by the board (fresh line + prompt proves it)
>
```

## Implementation Notes

- **IDLE detection vs. byte-by-byte interrupts** — a plain `HAL_UART_Receive_IT`
  needs the exact length up front; idle-line detection instead fires once per
  *frame* (data followed by ≥1 character time of silence), giving natural
  line-oriented semantics for a console.
- **The callback must re-arm reception** — `ReceiveToIdle_IT` is one-shot; without
  the trailing `HAL_UARTEx_ReceiveToIdle_IT()` inside the callback the board goes
  deaf after the first frame.
- **Callback context** — `HAL_UARTEx_RxEventCallback` runs in **interrupt context**;
  keep it short. The blocking `HAL_UART_Transmit` calls here are acceptable only
  because replies are tiny; long work in an ISR blocks lower-priority interrupts.
- **TIM3 is initialized but unused** — `MX_TIM3_Init()` is kept from the CubeMX
  configuration (inherited from earlier projects) but its interrupt is never
  started; it plays no role in the UART path.
- **Baud rate depends on the clock tree** — BRR is computed from PCLK1 (42 MHz);
  with the board's 8 MHz crystal, `PLLM = 8` and `HSE_VALUE = 8 MHz` must match or
  the terminal shows mojibake (see 005's README for the story).
- **Generated code** — the skeleton (startup, linker script, HAL drivers,
  `usart.c`, `tim.c`, clock config) is generated by **STM32CubeMX**; user code
  lives in the `USER CODE` sections so the project can be safely regenerated.

## Build & Flash

Build (Debug, default):

```sh
cmake --preset Debug && cmake --build --preset Debug
```

The resulting firmware is at `build/Debug/006-UartInterrupt.elf`.

To flash and run, press **F5** in VS Code (`cortex-debug` + OpenOCD) — it builds,
flashes via the DAP-Link probe and halts at `main`. You can also flash manually:

```sh
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
        -c "program build/Debug/006-UartInterrupt.elf verify reset exit"
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
    ├── main.c              # Entry point: banner + arm RX, echo in RxEventCallback
    ├── usart.c             # UART5 115200-8N1 init + PC12/PD2 AF8 GPIO config
    ├── stm32f4xx_it.c      # UART5_IRQHandler → HAL_UART_IRQHandler
    ├── tim.c               # TIM3 (unused by the UART path, kept from CubeMX)
    └── system_stm32f4xx.c  # System clock
```

## License

HAL and driver code is from STMicroelectronics (STM32F4xx HAL, CMSIS). See
`Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`.
