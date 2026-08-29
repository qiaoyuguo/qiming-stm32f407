/**
  ******************************************************************************
  * @file    eeprom.c
  * @brief   AT24C02 (24C02) EEPROM driver, based on hardware I2C1 (hi2c1).
  ******************************************************************************
  */
#include "eeprom.h"
#include "i2c.h"
#include <string.h>
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

/* Private define ------------------------------------------------------------*/
#define EEPROM_WRITE_CYCLE_MS   10u  /* max internal write cycle time */

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Probe the EEPROM on the bus.
  * @param  trials: number of probe attempts
  * @retval HAL status
  */
HAL_StatusTypeDef EEPROM_IsReady(uint32_t trials)
{
  return HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_DEV_ADDR, trials, EEPROM_TIMEOUT);
}

/**
  * @brief  Write one byte.
  */
HAL_StatusTypeDef EEPROM_WriteByte(uint16_t addr, uint8_t data)
{
  if (addr >= EEPROM_SIZE)
    return EEPROM_ERROR;

  HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, EEPROM_DEV_ADDR, addr,
                                           I2C_MEMADD_SIZE_8BIT, &data, 1,
                                           EEPROM_TIMEOUT);
  if (st != EEPROM_OK)
    return st;

  HAL_Delay(EEPROM_WRITE_CYCLE_MS);   /* wait internal write cycle */
  return EEPROM_OK;
}

/**
  * @brief  Read one byte.
  */
HAL_StatusTypeDef EEPROM_ReadByte(uint16_t addr, uint8_t *data)
{
  if (addr >= EEPROM_SIZE || data == NULL)
    return EEPROM_ERROR;

  return HAL_I2C_Mem_Read(&hi2c1, EEPROM_DEV_ADDR, addr,
                          I2C_MEMADD_SIZE_8BIT, data, 1, EEPROM_TIMEOUT);
}

/**
  * @brief  Write a buffer, splitting at page boundaries.
  *         Each page write is followed by a wait for the write cycle.
  */HAL_StatusTypeDef EEPROM_Write(uint16_t addr, uint8_t *buf, uint16_t len)
{
  if (addr + len > EEPROM_SIZE || buf == NULL)
    return EEPROM_ERROR;

  uint16_t remaining = len;

  while (remaining > 0u)
  {
    /* bytes we may write now: page size or to page end, whichever smaller */
    uint16_t chunk = EEPROM_PAGE_SIZE - (addr % EEPROM_PAGE_SIZE);
    if (chunk > remaining)
      chunk = remaining;

    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, EEPROM_DEV_ADDR, addr,
                                             I2C_MEMADD_SIZE_8BIT, buf, chunk,
                                             EEPROM_TIMEOUT);
    if (st != EEPROM_OK)
      return st;

    /* wait for internal write cycle to complete (max 10ms per datasheet) */
    HAL_Delay(EEPROM_WRITE_CYCLE_MS);

    addr      += chunk;
    buf       += chunk;
    remaining -= chunk;
  }

  return EEPROM_OK;
}

/**
  * @brief  Read a buffer sequentially (no page limit on reads).
  */
HAL_StatusTypeDef EEPROM_Read(uint16_t addr, uint8_t *buf, uint16_t len)
{
  if (addr + len > EEPROM_SIZE || buf == NULL)
    return EEPROM_ERROR;

  return HAL_I2C_Mem_Read(&hi2c1, EEPROM_DEV_ADDR, addr,
                          I2C_MEMADD_SIZE_8BIT, buf, len, EEPROM_TIMEOUT);
}

/**
  * @brief  Return page index of the given address.
  */
uint8_t EEPROM_PageOf(uint16_t addr)
{
  return (uint8_t)(addr / EEPROM_PAGE_SIZE);
}

/**
  * @brief  Return number of bytes remaining in the current page.
  */
uint16_t EEPROM_BytesToPageEnd(uint16_t addr)
{
  return EEPROM_PAGE_SIZE - (addr % EEPROM_PAGE_SIZE);
}

/**
  * @brief  Wait one EEPROM internal write cycle (fixed delay).
  */
HAL_StatusTypeDef EEPROM_WaitForWriteCycle(void)
{
  HAL_Delay(EEPROM_WRITE_CYCLE_MS);
  return EEPROM_OK;
}

/**
  * @brief  Fill len bytes at addr with a fixed pattern (page-aware).
  */
HAL_StatusTypeDef EEPROM_Fill(uint16_t addr, uint8_t pattern, uint16_t len)
{
  uint8_t page[EEPROM_PAGE_SIZE];
  memset(page, pattern, sizeof(page));

  uint16_t remaining = len;
  while (remaining > 0u)
  {
    uint16_t chunk = remaining;
    if (chunk > EEPROM_PAGE_SIZE)
      chunk = EEPROM_PAGE_SIZE;

    HAL_StatusTypeDef st = EEPROM_Write(addr, page, chunk);
    if (st != EEPROM_OK)
      return st;

    addr      += chunk;
    remaining -= chunk;
  }
  return EEPROM_OK;
}

/**
  * @brief  Erase the entire chip (all bytes -> 0xFF).
  */
HAL_StatusTypeDef EEPROM_EraseAll(void)
{
  return EEPROM_Fill(0, 0xFF, EEPROM_SIZE);
}

/**
  * @brief  Read back len bytes at addr and compare with buf.
  * @retval EEPROM_OK if identical, EEPROM_ERROR otherwise
  */
HAL_StatusTypeDef EEPROM_Verify(uint16_t addr, uint8_t *buf, uint16_t len)
{
  uint8_t tmp[EEPROM_PAGE_SIZE * 4];
  uint16_t remaining = len;

  while (remaining > 0u)
  {
    uint16_t chunk = remaining;
    if (chunk > sizeof(tmp))
      chunk = sizeof(tmp);

    if (EEPROM_Read(addr, tmp, chunk) != EEPROM_OK)
      return EEPROM_ERROR;
    if (memcmp(tmp, buf, chunk) != 0)
      return EEPROM_ERROR;

    addr      += chunk;
    buf       += chunk;
    remaining -= chunk;
  }
  return EEPROM_OK;
}

/**
  * @brief  CRC-16/CCITT (poly 0x1021, init 0xFFFF) over EEPROM content.
  */
uint16_t EEPROM_CRC16(uint16_t addr, uint16_t len)
{
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++)
  {
    uint8_t b;
    if (EEPROM_ReadByte((uint16_t)(addr + i), &b) != EEPROM_OK)
      break;
    crc ^= (uint16_t)b << 8;
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                            : (uint16_t)(crc << 1);
  }
  return crc;
}

/**
  * @brief  Hex dump of EEPROM content via printf (debug use).
  */
void EEPROM_Dump(uint16_t addr, uint16_t len)
{
  for (uint16_t row = 0; row < len; row += 16)
  {
    printf("%04X: ", addr + row);
    uint8_t line[16];
    uint16_t n = len - row;
    if (n > 16)
      n = 16;
    if (EEPROM_Read(addr + row, line, n) != EEPROM_OK)
    {
      printf("read error\r\n");
      return;
    }
    for (uint16_t i = 0; i < 16; i++)
    {
      if (i < n)
        printf("%02X ", line[i]);
      else
        printf("   ");
    }
    printf(" ");
    for (uint16_t i = 0; i < n; i++)
      printf("%c", (line[i] >= 0x20 && line[i] < 0x7F) ? line[i] : '.');
    printf("\r\n");
  }
}
