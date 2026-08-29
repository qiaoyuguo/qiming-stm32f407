/**
  ******************************************************************************
  * @file    w25q128.h
  * @brief   W25Q128 SPI Flash driver header (SPI1, CS = PG8)
  ******************************************************************************
  */
#ifndef __W25Q128_H
#define __W25Q128_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Device geometry */
#define W25Q128_PAGE_SIZE        256U                      /* bytes per page      */
#define W25Q128_SECTOR_SIZE      4096U                     /* 4KB sector          */
#define W25Q128_BLOCK32_SIZE     32768U                    /* 32KB block          */
#define W25Q128_BLOCK64_SIZE     65536U                    /* 64KB block          */
#define W25Q128_FLASH_SIZE       (16U * 1024U * 1024U)     /* 16MB total          */

/* JEDEC manufacturer / device ID expected: EFh 4018h */
#define W25Q128_MANUFACTURER_ID  0xEFU
#define W25Q128_DEVICE_ID        0x4018U
#define W25Q128_JEDEC_ID         (((uint32_t)W25Q128_MANUFACTURER_ID << 16) | W25Q128_DEVICE_ID)

/* Status register bits (SR1) */
#define W25Q128_SR1_BUSY         0x01U
#define W25Q128_SR1_WEL          0x02U

/* Return codes */
typedef enum
{
  W25Q128_OK      = 0,
  W25Q128_ERROR   = -1,
  W25Q128_TIMEOUT = -2,
  W25Q128_BAD_ID  = -3,
} W25Q128_Status_t;

/* API */
W25Q128_Status_t W25Q128_Init(void);
W25Q128_Status_t W25Q128_ReadID(uint32_t *id);
W25Q128_Status_t W25Q128_Read(uint32_t addr, uint8_t *buf, uint32_t len);
W25Q128_Status_t W25Q128_Write(uint32_t addr, const uint8_t *buf, uint32_t len);
W25Q128_Status_t W25Q128_EraseSector(uint32_t addr);
W25Q128_Status_t W25Q128_EraseBlock32K(uint32_t addr);
W25Q128_Status_t W25Q128_EraseBlock64K(uint32_t addr);
W25Q128_Status_t W25Q128_EraseChip(void);
W25Q128_Status_t W25Q128_ReadStatus(uint8_t *sr1);
W25Q128_Status_t W25Q128_ReadStatus2(uint8_t *sr2);
W25Q128_Status_t W25Q128_WaitBusy(uint32_t timeout_ms);
W25Q128_Status_t W25Q128_WriteEnable(void);
W25Q128_Status_t W25Q128_WriteDisable(void);
W25Q128_Status_t W25Q128_ReadUniqueID(uint8_t uid[8]);
W25Q128_Status_t W25Q128_PowerDown(void);
W25Q128_Status_t W25Q128_ReleasePowerDown(void);
W25Q128_Status_t W25Q128_LockBlock(uint32_t addr);      /* 64KB block lock   */
W25Q128_Status_t W25Q128_UnlockBlock(uint32_t addr);    /* 64KB block unlock */
W25Q128_Status_t W25Q128_ReadBlockLock(uint32_t addr, uint8_t *locked);
W25Q128_Status_t W25Q128_WriteStatusReg(uint8_t sr1);   /* set BP bits etc.  */

#ifdef __cplusplus
}
#endif

#endif /* __W25Q128_H */
