# 003-KeyByInterrupt (按键中断)

An **interrupt-driven key** example for the STM32F407ZGTx that reads four on-board push buttons using **GPIO EXTI interrupts** (falling-edge) instead of a blocking poll loop. Each button press triggers an interrupt handler that drives the LEDs. It demonstrates digital input handling with `GPIO_MODE_IT_FALLING`, NVIC configuration, and the HAL `HAL_GPIO_EXTI_IRQHandler` / `HAL_GPIO_EXTI_Callback` flow.

## Hardware Requirements

| Resource | Pin | Role |
| --- | --- | --- |
| KEY0 (UP) | PF9 (GPIOF pin 9) | EXTI input, pull-up, active low |
| KEY1 (LEFT) | PF8 (GPIOF pin 8) | EXTI input, pull-up, active low |
| KEY2 (DOWN) | PF7 (GPIOF pin 7) | EXTI input, pull-up, active low |
| KEY3 (RIGHT) | PF6 (GPIOF pin 6) | EXTI input, pull-up, active low |
| LED0 | PE3 (GPIOE pin 3) | Push-pull output |
| LED1 | PE4 (GPIOE pin 4) | Push-pull output |
| LED2 | PG9 (GPIOG pin 9) | Push-pull output |
| BEEP (buzzer) | PG7 (GPIOG pin 7) | Push-pull output |
| Debug probe | DAP‑Link via SWD (CMSIS‑DAP) | — |

All four keys are configured as inputs with internal **pull-up** resistors, so pressing a button pulls the pin low (a **falling edge**) and generates an EXTI interrupt. PF6–PF9 map to EXTI lines 6–9, all served by the shared **`EXTI9_5`** IRQ.

## Features

- System clock configured to **168 MHz** (HSE 25 MHz → PLL: M=25, N=336, P=2)
- Reads four push buttons via **GPIO EXTI falling-edge interrupts** — no blocking scan in the main loop
- NVIC interrupt `EXTI9_5_IRQn` enabled and prioritized for the key pins
- Each key drives a different action:
  - **KEY0** → LED0 **on**
  - **KEY1** → LED0 **off**
  - **KEY2** → LED2 **toggle**
  - **KEY3** → LED1 **toggle**
- Main loop stays **idle**; all key handling happens asynchronously in the interrupt callback
- Built and debugged directly from VS Code (CMake + cortex-debug)

## How It Works

The logic is split between `Core/Src/gpio.c` (configuration), `Core/Src/stm32f4xx_it.c` (IRQ handler) and `Core/Src/main.c` (callback action).

1. **Startup / initialization**
   - `HAL_Init()` — resets peripherals, initializes the Flash interface and SysTick.
   - `SystemClock_Config()` — drives the system to 168 MHz using the PLL.
   - `MX_GPIO_Init()` — configures LED0–LED2 / BEEP as push-pull outputs and KEY0–KEY3 as **EXTI falling-edge** inputs with pull-up, then enables the interrupt in the NVIC.

2. **GPIO + NVIC configuration (`gpio.c`)**:
   ```c
   GPIO_InitStruct.Pin  = KEY3_Pin|KEY2_Pin|KEY1_Pin|KEY0_Pin;
   GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;   // EXTI, falling edge
   GPIO_InitStruct.Pull = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

   // HAL_GPIO_Init does NOT enable the NVIC in this HAL — enable it explicitly
   HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
   HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
   ```

3. **IRQ handler (`stm32f4xx_it.c`)** — forwards the shared EXTI9_5 interrupt to each key pin:
   ```c
   void EXTI9_5_IRQHandler(void)
   {
     HAL_GPIO_EXTI_IRQHandler(KEY0_Pin);
     HAL_GPIO_EXTI_IRQHandler(KEY1_Pin);
     HAL_GPIO_EXTI_IRQHandler(KEY2_Pin);
     HAL_GPIO_EXTI_IRQHandler(KEY3_Pin);
   }
   ```
   `HAL_GPIO_EXTI_IRQHandler()` checks the pending flag for the given pin, clears it, and calls the weak `HAL_GPIO_EXTI_Callback()`.

4. **Callback (`main.c`)** — overrides the weak callback to run the LED actions:
   ```c
   void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
   {
     if (GPIO_Pin == KEY0_Pin)      HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
     else if (GPIO_Pin == KEY1_Pin) HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
     else if (GPIO_Pin == KEY2_Pin) HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
     else if (GPIO_Pin == KEY3_Pin) HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
   }
   ```

5. **Main loop** — left empty; the CPU is free to do other work while key presses are serviced by the interrupt:
   ```c
   while (1)
   {
     /* interrupt-driven; nothing to poll here */
   }
   ```

## Implementation Notes

- **Interrupt-driven vs. polling** — unlike the blocking `KeyScan` of `002-KeyScanByDelay`, this example uses **hardware EXTI interrupts**, so the main loop is never blocked while waiting for a key press.
- **Falling-edge with pull-up** — the buttons connect the pin to ground when pressed; with the internal pull-up enabled this creates a clean falling edge, so `GPIO_MODE_IT_FALLING` is used.
- **No software debouncing** — this version performs no explicit debounce. In real applications you would typically add one (e.g. a debounce timer, or re-check the level in the callback) to reject contact bounce. This is the natural next step over `002`.
- **Shared `EXTI9_5` IRQ** — PF6–PF9 all map to the same NVIC line, so the handler must forward to every key pin via `HAL_GPIO_EXTI_IRQHandler`.
- **NVIC enabled manually** — in this HAL the generated `gpio.c` enables `EXTI9_5_IRQn` explicitly because `HAL_GPIO_Init` does not enable the NVIC.
- **Callback runs in interrupt context** — keep the work short (just GPIO writes here) and avoid blocking calls or `HAL_Delay` inside the callback.
- **Generated code** — The project skeleton (startup file, linker script, HAL drivers, `gpio.c`, `stm32f4xx_it.c`, clock config) is generated by **STM32CubeMX** and kept under the `USER CODE` sections, so it can be safely regenerated without losing custom logic.

## Build & Flash

Build (Debug, default):

```sh
cmake --preset Debug && cmake --build --preset Debug
```

The resulting firmware is at `build/Debug/003-KeyByInterrupt.elf`.

To flash and run, press **F5** in VS Code (`cortex-debug` + OpenOCD) — it builds, flashes via the DAP‑Link probe and halts at `main`. You can also flash manually:

```sh
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
        -c "program build/Debug/003-KeyByInterrupt.elf verify reset exit"
```

## Project Layout (relevant files)

```
Core/
├── Inc/
│   ├── main.h              # Pin definitions (KEY0–KEY3, LED0–LED2, BEEP)
│   ├── gpio.h
│   └── stm32f4xx_it.h
└── Src/
    ├── main.c              # Entry point + HAL_GPIO_EXTI_Callback (LED actions)
    ├── gpio.c              # EXTI pin + NVIC configuration
    ├── stm32f4xx_it.c      # EXTI9_5_IRQHandler → HAL_GPIO_EXTI_IRQHandler
    └── system_stm32f4xx.c  # System clock
```

## License

HAL and driver code is from STMicroelectronics (STM32F4xx HAL, CMSIS). See `Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`.
