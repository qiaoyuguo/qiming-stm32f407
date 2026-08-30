# 010-UartDma — STM32F407 UART5 DMA Receive/Transmit

Bare-metal STM32F407ZGTx project (STM32CubeMX + CMake + arm-none-eabi-gcc) that
demonstrates **UART5 with DMA** on both directions:

- **RX**: `HAL_UARTEx_ReceiveToIdle_DMA()` — DMA moves incoming bytes into a
  buffer automatically, and the IDLE-line event ends a "frame" so variable-length
  packets can be received with zero CPU cost per byte.
- **TX**: `HAL_UART_Transmit_DMA()` — non-blocking transmit with a `printf`-style
  wrapper (`uart_dma_printf`).

The application echoes back anything received on UART5 and prints the byte count.

---

## 1. Hardware Configuration

### 1.1 MCU & Peripherals

| Item | Value |
|---|---|
| MCU | STM32F407ZGTx, LQFP144 |
| System clock | HSE 8MHz → PLL → 168MHz (APB2 = 84MHz) |
| Debug | DAP-Link + OpenOCD (SWD) |
| UART | UART5, 115200-8-N-1 |
| DMA | DMA1 Stream0 (RX) / Stream7 (TX), Channel 4, NORMAL mode |

### 1.2 UART5 Pin Mapping

| Signal | Pin | AF |
|---|---|---|
| UART5_TX | PC12 | AF8 |
| UART5_RX | PD2 | AF8 |

UART5 is on APB1 (42 MHz) → BRR for 115200 baud.

### 1.3 Buffer Sizes (Core/Inc/usart.h)

| Buffer | Size | Purpose |
|---|---|---|
| `uart_rx_buf` | 128 B | DMA RX circular landing buffer (one frame per reception) |
| `uart_tx_buf` | 256 B | Formats `uart_dma_printf` output before DMA TX |

---

## 2. Software Design

### 2.1 Reception Chain

```
UART5 RX byte ──(DMA1_Stream0, CH4)──> uart_rx_buf
        │
        ├── buffer full (HT disabled, only TC) ──┐
        └── bus idle > 1 character time ─────────┤
                                                 ▼
                        HAL_UARTEx_RxEventCallback(UART5, Size)
                                 │
                        uart_rx_len = Size; uart_rx_flag = 1;
                        uart5_start_rx_dma();   // re-arm DMA (NORMAL mode)
```

Key points:

- `__HAL_DMA_DISABLE_IT(huart5.hdmarx, DMA_IT_HT)` disables the **half-transfer**
  interrupt so the callback fires only on frame end (IDLE or full buffer), not at
  the buffer midpoint.
- DMA is in **NORMAL** (not CIRCULAR) mode, so each `RxEventCallback` must
  re-start reception. This lets `HAL_UARTEx_ReceiveToIdle_DMA` be re-armed with
  a fresh, empty buffer — no partial-frame bookkeeping.

### 2.2 Transmission

- `uart_dma_transmit()` sets `uart_tx_busy` and calls `HAL_UART_Transmit_DMA()`.
- `HAL_UART_TxCpltCallback()` clears `uart_tx_busy` when DMA finishes.
- `uart_dma_printf()` formats with `vsnprintf` into a static 256 B buffer, then
  submits via DMA — a lightweight `printf` console with almost no CPU time in TX.
- Main loop copies the RX frame out before re-arming, so echo processing never
  races the next reception.

---

## 3. Problems Encountered During Development & Fixes

| # | Problem | Symptom | Root cause | Fix |
|---|---|---|---|---|
| 1 | `RxEventCallback` fired mid-frame | A 20-byte frame produced callbacks of 64 bytes (half buffer) before the frame finished | DMA **half-transfer (HT)** interrupt was enabled by default in `HAL_UARTEx_ReceiveToIdle_DMA` | Disabled it: `__HAL_DMA_DISABLE_IT(huart5.hdmarx, DMA_IT_HT)` right after starting reception |
| 2 | Only the **first** frame was ever received | Echo worked once, then UART went silent | DMA configured in **NORMAL** mode; after transfer complete the stream is disabled and never re-armed | Re-call `uart5_start_rx_dma()` inside `HAL_UARTEx_RxEventCallback()` to re-arm reception for every frame |
| 3 | Overrun errors killed reception | After sending data fast/back-to-back, no more echoes even though the link was fine | RX bytes arrived while DMA was momentarily not re-armed → **ORE** (overrun) error flag set; HAL aborted RX | Implemented `HAL_UART_ErrorCallback()`: clear `__HAL_UART_CLEAR_OREFLAG(huart)` then restart `uart5_start_rx_dma()` so the RX chain self-recovers |
| 4 | Second `uart_dma_printf` overwrote a still-in-flight DMA TX buffer | Occasional corrupted/garbled output lines | `HAL_UART_Transmit_DMA` returns immediately; starting a new transfer re-points the DMA to a buffer the previous transfer was still reading | Added a `uart_tx_busy` flag set on submit and cleared in `HAL_UART_TxCpltCallback()`; `uart_dma_transmit()` busy-waits on it before starting a new transfer |
| 5 | Long `printf` output truncated/garbled if called before formatting done | Tail of long lines missing or mixed between calls | Single shared TX buffer + `vsnprintf` length not clamped | Clamped `len` to `UART_TX_BUF_SIZE` after `vsnprintf` and return `-1` on formatting error |
| 6 | RX flag race with main loop | Rarely, a new frame would overwrite `uart_rx_buf` while the main loop was still echoing the previous one | Callback wrote directly into the shared buffer while main loop read it | Main loop first copies `uart_rx_buf` into a local `echo_buf` and clears `uart_rx_flag` before processing; new frames only overwrite after that |

---

## 4. Build & Run

```bash
cd 010-UartDma
cmake --preset Debug && cmake --build --preset Debug
# Flash via OpenOCD / DAP-Link (or use the VS Code F5 debug task)
```

Expected UART5 output (115200-8-N-1):

```
[010-UartDma] UART5 DMA test start (115200-8-N-1)
Send any data to UART5 (PC12-TX / PD2-RX), it will be echoed back.
Echo 5 bytes: hello
```

Send any bytes to UART5; each frame (ended by an idle gap) is echoed back with
its byte count.

---

## 5. Key Files

| File | Content |
|---|---|
| `Core/Src/usart.c` | DMA RX/TX API (`uart5_start_rx_dma`, `uart_dma_transmit`, `uart_dma_printf`) and UART callbacks |
| `Core/Src/dma.c` | DMA1 Stream0/Stream7 init and NVIC config |
| `Core/Src/main.c` | Test loop: start RX, print banner, echo frames |
| `Core/Inc/usart.h` | Buffer sizes and public API declarations |
