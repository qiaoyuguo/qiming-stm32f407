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
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "w25q128.h"
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
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  uint32_t flash_id = 0;
  if (W25Q128_ReadID(&flash_id) == W25Q128_OK && flash_id == W25Q128_JEDEC_ID)
  {
    printf("[TEST0 readID] pass: JEDEC ID = 0x%06lX\r\n", (unsigned long)flash_id);
  }
  else
  {
    printf("[TEST0 readID] failed: JEDEC ID = 0x%06lX\r\n", (unsigned long)flash_id);
  }

  /* ---- W25Q128 API verification ---- */
  static uint8_t tx_buf[4096];
  static uint8_t rx_buf[4096];
  int pass_cnt = 0;
  int fail_cnt = 0;

  /* Test 0: erase sectors 0 and 1 (0x0000~0x1FFF) as test range */
  if (W25Q128_EraseSector(0) == W25Q128_OK &&
      W25Q128_EraseSector(4096) == W25Q128_OK)
  {
    printf("[ERASE] ok\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[ERASE] failed\r\n");
    fail_cnt++;
  }

  /* Test 1: write 32 bytes at 0x10, verify */
  for (int i = 0; i < 32; i++)
  {
    tx_buf[i] = (uint8_t)(0xA0 + i);
  }
  if (W25Q128_Write(0x10, tx_buf, 32) == W25Q128_OK &&
      W25Q128_Read(0x10, rx_buf, 32) == W25Q128_OK &&
      memcmp(tx_buf, rx_buf, 32) == 0)
  {
    printf("[TEST1 32B@0x10] pass\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[TEST1 32B@0x10] failed\r\n");
    fail_cnt++;
  }

  /* Test 2: write 3 pages (768B, unaligned start crossing page boundary) at 0x80 */
  for (int i = 0; i < 768; i++)
  {
    tx_buf[i] = (uint8_t)(i & 0xFF);
  }
  if (W25Q128_Write(0x80, tx_buf, 768) == W25Q128_OK &&
      W25Q128_Read(0x80, rx_buf, 768) == W25Q128_OK &&
      memcmp(tx_buf, rx_buf, 768) == 0)
  {
    printf("[TEST2 3pages@0x80] pass\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[TEST2 3pages@0x80] failed\r\n");
    fail_cnt++;
  }

  /* Test 3: write 1 full sector (4096B) at 0x800, spanning sectors 0/1 */
  for (uint32_t i = 0; i < 4096; i++)
  {
    tx_buf[i] = (uint8_t)(i * 7);
  }
  if (W25Q128_Write(0x800, tx_buf, 4096) == W25Q128_OK &&
      W25Q128_Read(0x800, rx_buf, 4096) == W25Q128_OK &&
      memcmp(tx_buf, rx_buf, 4096) == 0)
  {
    printf("[TEST3 1sector@0x800] pass\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[TEST3 1sector@0x800] failed\r\n");
    fail_cnt++;
  }

  /* Test 4: erase sector 1 (0x1000) and verify all 0xFF */
  uint8_t ff_ok = 1;
  if (W25Q128_EraseSector(0x1000) == W25Q128_OK &&
      W25Q128_Read(0x1000, rx_buf, 4096) == W25Q128_OK)
  {
    for (uint32_t i = 0; i < 4096; i++)
    {
      if (rx_buf[i] != 0xFF)
      {
        ff_ok = 0;
        break;
      }
    }
  }
  else
  {
    ff_ok = 0;
  }
  if (ff_ok)
  {
    printf("[TEST4 erase&checkFF@0x1000] pass\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[TEST4 erase&checkFF@0x1000] failed\r\n");
    fail_cnt++;
  }

  /* Test 5: write across sector boundary 0xFFE~0x1002 (spans sectors 0 and 1)
   * NOTE: sector 0 was programmed by TEST1~3, re-erase it first because
   * flash programming can only clear bits (1 -> 0), never set them. */
  uint8_t cross_tx[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78};
  if (W25Q128_EraseSector(0) == W25Q128_OK &&
      W25Q128_Write(0x0FFE, cross_tx, 8) == W25Q128_OK &&
      W25Q128_Read(0x0FFE, rx_buf, 8) == W25Q128_OK &&
      memcmp(cross_tx, rx_buf, 8) == 0)
  {
    printf("[TEST5 cross-sector@0xFFE] pass\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[TEST5 cross-sector@0xFFE] failed\r\n");
    fail_cnt++;
  }

  /* Test 6: WriteEnable -> WEL=1, WriteDisable -> WEL=0 */
  uint8_t sr1 = 0;
  uint8_t wel_ok = 0;
  if (W25Q128_WriteEnable() == W25Q128_OK &&
      W25Q128_ReadStatus(&sr1) == W25Q128_OK &&
      (sr1 & W25Q128_SR1_WEL) != 0U)
  {
    if (W25Q128_WriteDisable() == W25Q128_OK &&
        W25Q128_ReadStatus(&sr1) == W25Q128_OK &&
        (sr1 & W25Q128_SR1_WEL) == 0U)
    {
      wel_ok = 1;
    }
  }
  if (wel_ok)
  {
    printf("[TEST6 writedisable] pass\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[TEST6 writedisable] failed\r\n");
    fail_cnt++;
  }

  /* Test 7: read status register 2 */
  uint8_t sr2 = 0xFF;
  if (W25Q128_ReadStatus2(&sr2) == W25Q128_OK)
  {
    printf("[TEST7 status2] pass: SR2 = 0x%02X\r\n", sr2);
    pass_cnt++;
  }
  else
  {
    printf("[TEST7 status2] failed\r\n");
    fail_cnt++;
  }

  /* Test 8: read unique ID twice, both reads equal and not all 0xFF */
  uint8_t uid1[8] = {0}, uid2[8] = {0};
  uint8_t uid_ok = 0;
  if (W25Q128_ReadUniqueID(uid1) == W25Q128_OK &&
      W25Q128_ReadUniqueID(uid2) == W25Q128_OK &&
      memcmp(uid1, uid2, 8) == 0)
  {
    uid_ok = 1;
    for (int i = 0; i < 8; i++)
    {
      if (uid1[i] != 0xFF)
      {
        break;
      }
      if (i == 7)
      {
        uid_ok = 0;
      }
    }
  }
  if (uid_ok)
  {
    printf("[TEST8 uniqueid] pass: %02X%02X%02X%02X%02X%02X%02X%02X\r\n",
           uid1[0], uid1[1], uid1[2], uid1[3],
           uid1[4], uid1[5], uid1[6], uid1[7]);
    pass_cnt++;
  }
  else
  {
    printf("[TEST8 uniqueid] failed\r\n");
    fail_cnt++;
  }

  /* Test 9: power down, release, then JEDEC ID readable again */
  uint32_t id_after = 0;
  uint8_t pd_ok = 0;
  if (W25Q128_PowerDown() == W25Q128_OK &&
      W25Q128_ReleasePowerDown() == W25Q128_OK &&
      W25Q128_ReadID(&id_after) == W25Q128_OK &&
      id_after == W25Q128_JEDEC_ID)
  {
    pd_ok = 1;
  }
  if (pd_ok)
  {
    printf("[TEST9 powerdown] pass\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[TEST9 powerdown] failed\r\n");
    fail_cnt++;
  }

  /* Test 10: 64KB block lock protection
   * erase sector at 0x10000, lock its 64KB block, write attempt must be
   * ignored (read back 0xFF), unlock then write must succeed. */
  /* Test 10: BP-based global write protection (works on all W25Q/GD25)
   * BP0~BP2 = 1 (SR1 = 0x1C) protects the whole array from programming.
   * Note: erase is also blocked while BP bits are set. */
  uint8_t lock_ok = 0;
  do
  {
    if (W25Q128_EraseSector(0x10000) != W25Q128_OK)
    {
      printf("  step1 erase failed\r\n");
      break;
    }
    /* enable protection */
    if (W25Q128_WriteStatusReg(0x1C) != W25Q128_OK)
    {
      printf("  step2 set BP failed\r\n");
      break;
    }
    W25Q128_ReadStatus(&sr1);
    printf("  SR1 = 0x%02X\r\n", sr1);
    for (int i = 0; i < 32; i++)
    {
      tx_buf[i] = (uint8_t)(0x5A + i);
    }
    if (W25Q128_Write(0x10100, tx_buf, 32) != W25Q128_OK)
    {
      printf("  step3 write cmd failed\r\n");
      break;
    }
    if (W25Q128_Read(0x10100, rx_buf, 32) != W25Q128_OK)
    {
      printf("  step4 read failed\r\n");
      break;
    }
    uint8_t all_ff = 1;
    for (int i = 0; i < 32; i++)
    {
      if (rx_buf[i] != 0xFF)
      {
        all_ff = 0;
        break;
      }
    }
    if (!all_ff)
    {
      printf("  step5 data written despite BP (rx[0]=0x%02X)\r\n", rx_buf[0]);
      break;
    }
    /* clear protection and write again, must succeed */
    if (W25Q128_WriteStatusReg(0x00) != W25Q128_OK ||
        W25Q128_Write(0x10100, tx_buf, 32) != W25Q128_OK ||
        W25Q128_Read(0x10100, rx_buf, 32) != W25Q128_OK ||
        memcmp(tx_buf, rx_buf, 32) != 0)
    {
      printf("  step6 unprotect-write failed\r\n");
      break;
    }
    lock_ok = 1;
  } while (0);
  if (lock_ok)
  {
    printf("[TEST10 blocklock] pass\r\n");
    pass_cnt++;
  }
  else
  {
    printf("[TEST10 blocklock] failed\r\n");
    fail_cnt++;
  }

  printf("==== result: %d pass, %d fail ====\r\n", pass_cnt, fail_cnt);
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
