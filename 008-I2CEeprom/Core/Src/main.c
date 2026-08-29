/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "eeprom.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart5, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/* software CRC-16/CCITT over a RAM buffer, used to cross-check EEPROM_CRC16 */
static uint16_t SoftCRC16(const uint8_t *buf, uint16_t len)
{
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++)
  {
    crc ^= (uint16_t)buf[i] << 8;
    for (uint8_t b = 0; b < 8; b++)
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                            : (uint16_t)(crc << 1);
  }
  return crc;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM3_Init();
  MX_UART5_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  /* --- Step 1: single byte write/read --- */
  uint8_t w1 = 0xA5, r1 = 0;
  if (EEPROM_WriteByte(0x00, w1) != EEPROM_OK)
    printf("Step1 (1 byte write) : FAIL (I2C error)\r\n");
  else if (EEPROM_ReadByte(0x00, &r1) != EEPROM_OK)
    printf("Step1 (1 byte read)  : FAIL (I2C error)\r\n");
  else
    printf("Step1 (1 byte)       : %s (W=0x%02X R=0x%02X)\r\n",
           (w1 == r1) ? "PASS" : "FAIL (mismatch)", w1, r1);

  /* --- Step 2: 4 byte write/read (within one page) --- */
  uint8_t w4[4] = {0x11, 0x22, 0x33, 0x44};
  uint8_t r4[4] = {0};
  if (EEPROM_Write(0x10, w4, sizeof(w4)) != EEPROM_OK)
    printf("Step2 (4 byte write) : FAIL (I2C error)\r\n");
  else if (EEPROM_Read(0x10, r4, sizeof(r4)) != EEPROM_OK)
    printf("Step2 (4 byte read)  : FAIL (I2C error)\r\n");
  else
    printf("Step2 (4 byte)       : %s\r\n",
           (memcmp(w4, r4, sizeof(w4)) == 0) ? "PASS" : "FAIL (mismatch)");

  /* --- Step 4: block fill + verify --- */
  uint8_t expect[16];
  memset(expect, 0x5A, sizeof(expect));
  printf("Step3.1 Fill 0x5A @0x20 len=16 : %s\r\n",
         (EEPROM_Fill(0x20, 0x5A, 16) == EEPROM_OK) ? "PASS" : "FAIL");
  printf("Step3.2 Verify 0x5A @0x20      : %s\r\n",
         (EEPROM_Verify(0x20, expect, sizeof(expect)) == EEPROM_OK) ? "PASS"
                                                                    : "FAIL");

  /* --- Step 4: CRC-16 of written data (EEPROM vs RAM cross-check) --- */
  uint8_t w10[10] = {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xA9};
  EEPROM_Write(0x00, w10, sizeof(w10));
  uint16_t crcE = EEPROM_CRC16(0x00, sizeof(w10));
  uint16_t crcS = SoftCRC16(w10, sizeof(w10));
  printf("Step4   CRC16 (eeprom=0x%04X soft=0x%04X) : %s\r\n",
         crcE, crcS, (crcE == crcS) ? "PASS" : "FAIL");

  /* --- Step 5: dump written region --- */
  printf("Step5   Dump first 4 pages:\r\n");
  EEPROM_Dump(0x00, 32);

  /* --- Step 6: cross-page 10-byte write + verify (uses Verify API) --- */
  printf("Step6   Cross-page 10B w+verify : %s\r\n",
         (EEPROM_Verify(0x00, w10, sizeof(w10)) == EEPROM_OK) ? "PASS"
                                                              : "FAIL");

  /* --- Step 7: erase whole chip at the end, check blank --- */
  printf("Step7.1 EraseAll               : %s\r\n",
         (EEPROM_EraseAll() == EEPROM_OK) ? "PASS" : "FAIL");
  memset(expect, 0xFF, 16);
  printf("Step7.2 Verify 0xFF @0x00      : %s\r\n",
         (EEPROM_Verify(0x00, expect, 16) == EEPROM_OK) ? "PASS" : "FAIL");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
