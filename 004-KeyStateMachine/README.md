# 004-KeyStateMachine (按键状态机)

A **key state machine** example for the STM32F407ZGTx that reads four on-board push buttons using a **periodic TIM3 interrupt** that drives a per-key software state machine. Unlike the delay-based scan of `002-KeyScanByDelay` (blocking) and the EXTI approach of `003-KeyByInterrupt` (interrupt-on-edge), this project decouples *scanning* from *handling*: a fixed 10 ms timer tick scans all keys through a small finite state machine, and events are consumed afterwards. This gives clean debouncing and a non-blocking, modular structure that is easy to extend.

## Hardware Requirements

| Resource | Pin | Role |
| --- | --- | --- |
| KEY0 (UP) | PF9 (GPIOF pin 9) | Input, pull-up, active low |
| KEY1 (LEFT) | PF8 (GPIOF pin 8) | Input, pull-up, active low |
| KEY2 (DOWN) | PF7 (GPIOF pin 7) | Input, pull-up, active low |
| KEY3 (RIGHT) | PF6 (GPIOF pin 6) | Input, pull-up, active low |
| LED0 | PE3 (GPIOE pin 3) | Push-pull output |
| LED1 | PE4 (GPIOE pin 4) | Push-pull output |
| LED2 | PG9 (GPIOG pin 9) | Push-pull output |
| BEEP (buzzer) | PG7 (GPIOG pin 7) | Push-pull output |
| TIM3 | APB1 (84 MHz, timer clock 168 MHz) | 10 ms periodic interrupt source |
| Debug probe | DAP‑Link via SWD (CMSIS‑DAP) | — |

All four keys are configured as inputs with internal **pull-up** resistors, so pressing a button pulls the pin low. The keys are **polled** (not edge-interrupt) inside the TIM3 callback, so no EXTI/NVIC configuration is needed for the keys themselves.

## Features

- System clock configured to **168 MHz** (HSE 25 MHz → PLL: M=25, N=336, P=2)
- **TIM3 periodic interrupt** (10 ms) drives the entire key-handling machinery in `HAL_TIM_PeriodElapsedCallback`
- **Per-key software state machine** with three states — `KEY_RELEASED`, `KEY_DEBOUNCING`, `KEY_PRESSED` — giving robust debouncing without `HAL_Delay` blocking
- **Event-based handling** — the scanner sets a `KEY_EVENT_PRESSED` event; a separate `Key_Action()` consumes it. Scanning and handling are cleanly separated
- Timer configuration: `Prescaler = 1679`, `Period = 999` → 168 MHz / 1680 / 1000 = **100 Hz (10 ms)**
- Debounce window of **30 ms** (three 10 ms ticks) rejects contact bounce before a press is accepted
- Each key drives a different action:
  - **KEY0** → LED0 **toggle**
  - **KEY1** → LED1 **toggle**
  - **KEY2** → LED2 **toggle**
  - **KEY3** → reserved (currently does nothing — easy extension point)
- Built and debugged directly from VS Code (CMake + cortex-debug)

## How It Works

The logic is split between `Core/Src/gpio.c` (pin configuration), `Core/Src/tim.c` (TIM3 setup) and `Core/Src/main.c` (state machine, scanning, actions).

1. **Startup / initialization**
   - `HAL_Init()` — resets peripherals, initializes the Flash interface and SysTick.
   - `SystemClock_Config()` — drives the system to 168 MHz using the PLL.
   - `MX_GPIO_Init()` — configures LED0–LED2 / BEEP as push-pull outputs and KEY0–KEY3 as pull-up inputs (plain `GPIO_MODE_INPUT`, no EXTI).
   - `MX_TIM3_Init()` — configures TIM3 for a 10 ms update interrupt, and `HAL_TIM_Base_Start_IT(&htim3)` starts it.

2. **Timer configuration (`tim.c`)** — TIM3 runs on the APB1 timer clock (168 MHz):
   ```c
   htim3.Init.Prescaler     = 1679;
   htim3.Init.CounterMode   = TIM_COUNTERMODE_UP;
   htim3.Init.Period        = 999;
   // 168 MHz / (1679+1) / (999+1) = 100 Hz  →  every 10 ms an update interrupt
   ```

3. **Key data structures (`main.c`)** — each key is represented by a struct holding its port/pin, current state, last-press timestamp and a pending event:
   ```c
   enum KeyState { KEY_RELEASED = 0, KEY_DEBOUNCING, KEY_PRESSED };
   enum KeyEvent { KEY_EVENT_NONE = 0, KEY_EVENT_PRESSED };

   typedef struct KeyInfo {
     GPIO_TypeDef *gpio;
     uint16_t pin;
     enum KeyState state;
     uint32_t lastPressTime;
     enum KeyEvent event;
   } KeyInfo;
   ```
   A `keys[]` array is initialized with all four buttons.

4. **State machine — per-key scan (`main.c`)** — `Key_Scan_Single()` is called for each key every tick:
   - `KEY_RELEASED`: record the current tick; if the pin reads low (`RESET`), move to `KEY_DEBOUNCING`.
   - `KEY_DEBOUNCING`: if the pin is still low and at least **30 ms** have elapsed, accept the press → `KEY_PRESSED`; otherwise (bounce ended, pin high again) return to `KEY_RELEASED`.
   - `KEY_PRESSED`: wait for the pin to go high (`SET`) — i.e. the button is released — then return to `KEY_RELEASED` and set `event = KEY_EVENT_PRESSED`.

   This confirms the press *and* the release, so bounce on both edges is filtered out.

   `Key_Scan()` simply loops over all keys:
   ```c
   void Key_Scan(void)
   {
     for (int i = 0; i < LEN_OF_ARRAY(keys); ++i)
       Key_Scan_Single(&keys[i]);
   }
   ```

5. **Event handling (`main.c`)** — `Key_Action()` consumes pending events and drives the LEDs. It clears the event first so each press fires exactly once:
   ```c
   void Key_Action(void)
   {
     for (int i = 0; i < LEN_OF_ARRAY(keys); ++i) {
       if (keys[i].event == KEY_EVENT_PRESSED) {
         keys[i].event = KEY_EVENT_NONE;
         switch (i) {
           case 0: HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin); break;
           case 1: HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin); break;
           case 2: HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin); break;
           case 3: /* reserved */                                break;
         }
       }
     }
   }
   ```

6. **Timer callback** — every 10 ms the TIM3 update interrupt calls the weak `HAL_TIM_PeriodElapsedCallback`, which overrides it to scan then act:
   ```c
   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
   {
     if (htim->Instance == TIM3) {
       Key_Scan();     // advance every key through the state machine (10 ms tick)
       Key_Action();   // consume events → drive LEDs
     }
   }
   ```

7. **Main loop** — left empty; all key handling is driven by the timer interrupt, so the CPU is free and no blocking delays are used:
   ```c
   while (1)
   {
     /* timer-interrupt driven; nothing to poll here */
   }
   ```

## Implementation Notes

- **State machine vs. polling/interrupt** —
  - vs. `002-KeyScanByDelay` (blocking `HAL_Delay` scan): this design is **non-blocking**; the scan runs on a timer tick.
  - vs. `003-KeyByInterrupt` (raw EXTI falling-edge): this design needs **no EXTI/NVIC** for the keys and produces a debounced *press* event (edge triggers need extra software debounce and can easily double-fire).
- **Fixed 10 ms scheduling** — the scan rate is set by the timer, not by unpredictable button edges, so debounce timing is deterministic and easy to tune (`Prescaler`/`Period`).
- **Debouncing across both edges** — the `KEY_PRESSED` state waits for the release, so contact bounce on press *and* release is filtered. The 30 ms window (~3 ticks) comfortably exceeds typical switch bounce.
- **Separation of scan vs. action** — `Key_Scan()` only updates key state/events; `Key_Action()` only reacts. This makes it trivial to add repeat (long-press / auto-repeat), multi-key combos, or move handling out of the ISR.
- **Event semantics** — events are generated on the *release* edge (pin returns high), so a single press produces exactly one `KEY_EVENT_PRESSED`.
- **Runs in interrupt context** — the work in the callback is short (a few pin reads + GPIO writes). For heavier handling, drain events in the main loop instead.
- **Generated code** — The project skeleton (startup file, linker script, HAL drivers, `gpio.c`, `tim.c`, `stm32f4xx_it.c`, clock config) is generated by **STM32CubeMX** and kept under the `USER CODE` sections, so it can be safely regenerated without losing custom logic.

## Build & Flash

Build (Debug, default):

```sh
cmake --preset Debug && cmake --build --preset Debug
```

The resulting firmware is at `build/Debug/004-KeyStateMachine.elf`.

To flash and run, press **F5** in VS Code (`cortex-debug` + OpenOCD) — it builds, flashes via the DAP‑Link probe and halts at `main`. You can also flash manually:

```sh
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
        -c "program build/Debug/004-KeyStateMachine.elf verify reset exit"
```

## Project Layout (relevant files)

```
Core/
├── Inc/
│   ├── main.h              # Pin definitions (KEY0–KEY3, LED0–LED2, BEEP)
│   ├── gpio.h
│   ├── tim.h
│   └── stm32f4xx_it.h
└── Src/
    ├── main.c              # Entry point + key state machine, Key_Scan, Key_Action,
    │                       #   HAL_TIM_PeriodElapsedCallback
    ├── gpio.c              # LED / key pin configuration
    ├── tim.c               # TIM3 10 ms periodic interrupt configuration
    ├── stm32f4xx_it.c      # TIM3_IRQHandler → HAL_TIM_IRQHandler
    └── system_stm32f4xx.c  # System clock
```

## License

HAL and driver code is from STMicroelectronics (STM32F4xx HAL, CMSIS). See `Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`.
