/**
  ******************************************************************************
  * @file    eeprom.h
  * @brief   AT24C02 (24C02) EEPROM driver header, based on hardware I2C1.
  *
  *  - Capacity : 256 bytes (address 0x00 ~ 0xFF)
  *  - Page size: 8 bytes, page write must not cross page boundary
  *  - Write cycle: ~5 ms, device will NACK during internal write,
  *    we poll with HAL_I2C_IsDeviceReady() until ACK.
  ******************************************************************************
  */
#ifndef __EEPROM_H
#define __EEPROM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Exported constants --------------------------------------------------------*/
#define EEPROM_DEV_ADDR     (0x50 << 1)   /* 7-bit device address 0x50, HAL needs 8-bit */
#define EEPROM_SIZE         256           /* total capacity in bytes  */
#define EEPROM_PAGE_SIZE    8             /* page size in bytes       */
#define EEPROM_PAGE_COUNT   (EEPROM_SIZE / EEPROM_PAGE_SIZE)
#define EEPROM_TIMEOUT      100           /* I2C transfer timeout, ms */
#define EEPROM_WRITE_CYCLE_MS  10         /* max internal write cycle, ms */

/* HAL status re-define for user convenience */
#define EEPROM_OK           HAL_OK
#define EEPROM_ERROR        HAL_ERROR
#define EEPROM_BUSY         HAL_BUSY
#define EEPROM_TIMEOUT_ERR  HAL_TIMEOUT

/* Exported functions --------------------------------------------------------*/

/* --- Device level ---------------------------------------------------------- */
HAL_StatusTypeDef EEPROM_IsReady(uint32_t trials);   /* probe device on the bus */
HAL_StatusTypeDef EEPROM_WaitForWriteCycle(void);    /* delay one write cycle   */

/* --- Single byte access ---------------------------------------------------- */
HAL_StatusTypeDef EEPROM_WriteByte(uint16_t addr, uint8_t data);
HAL_StatusTypeDef EEPROM_ReadByte(uint16_t addr, uint8_t *data);

/* --- Multi-byte access ----------------------------------------------------- */
HAL_StatusTypeDef EEPROM_Write(uint16_t addr, uint8_t *buf, uint16_t len); /* page-aware */
HAL_StatusTypeDef EEPROM_Read(uint16_t addr, uint8_t *buf, uint16_t len);

/* --- Block operations ------------------------------------------------------ */
HAL_StatusTypeDef EEPROM_Fill(uint16_t addr, uint8_t pattern, uint16_t len);
                                          /* fill len bytes with a fixed pattern */
HAL_StatusTypeDef EEPROM_EraseAll(void);  /* fill entire chip with 0xFF */
HAL_StatusTypeDef EEPROM_Verify(uint16_t addr, uint8_t *buf, uint16_t len);
                                          /* read back and compare with buf,
                                             return EEPROM_OK if identical     */

/* --- Helper / test utilities ------------------------------------------------ */
uint8_t  EEPROM_PageOf(uint16_t addr);          /* page index of addr           */
uint16_t EEPROM_BytesToPageEnd(uint16_t addr);  /* bytes left in current page   */
uint16_t EEPROM_CRC16(uint16_t addr, uint16_t len);
                                          /* CRC-16/CCITT over EEPROM content   */
void     EEPROM_Dump(uint16_t addr, uint16_t len);
                                          /* hex dump via printf (debug use)    */

#ifdef __cplusplus
}
#endif

#endif /* __EEPROM_H */
