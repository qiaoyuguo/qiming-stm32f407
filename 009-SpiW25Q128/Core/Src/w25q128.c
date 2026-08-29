/**
  ******************************************************************************
  * @file    w25q128.c
  * @brief   W25Q128 SPI Flash driver (SPI1, software CS on PG8)
  ******************************************************************************
  */
#include "main.h"
#include "spi.h"
#include "w25q128.h"

/* -------------------------------------------------------------------------- */
/* Command set                                                                */
/* -------------------------------------------------------------------------- */
#define CMD_WRITE_ENABLE     0x06U
#define CMD_WRITE_DISABLE    0x04U
#define CMD_READ_STATUS1     0x05U
#define CMD_READ_STATUS2     0x35U
#define CMD_WRITE_STATUS     0x01U
#define CMD_READ_DATA        0x03U
#define CMD_FAST_READ        0x0BU
#define CMD_PAGE_PROGRAM     0x02U
#define CMD_SECTOR_ERASE_4K  0x20U
#define CMD_BLOCK_ERASE_32K  0x52U
#define CMD_BLOCK_ERASE_64K  0xD8U
#define CMD_CHIP_ERASE       0xC7U
#define CMD_JEDEC_ID         0x9FU
#define CMD_DEV_ID           0x90U
#define CMD_UNIQUE_ID        0x4BU
#define CMD_POWER_DOWN       0xB9U
#define CMD_RELEASE_POWERDOWN 0xABU
#define CMD_INDIVIDUAL_LOCK    0x36U
#define CMD_INDIVIDUAL_UNLOCK  0x39U
#define CMD_READ_BLOCK_LOCK    0x3DU

#define BUSY_POLL_MS         500U    /* default wait-busy timeout          */
#define DUMMY_BYTE           0xFFU

/* -------------------------------------------------------------------------- */
/* Low level helpers                                                          */
/* -------------------------------------------------------------------------- */
static inline void CS_Low(void)
{
  HAL_GPIO_WritePin(W25Q128_CS_GPIO_Port, W25Q128_CS_Pin, GPIO_PIN_RESET);
}

static inline void CS_High(void)
{
  HAL_GPIO_WritePin(W25Q128_CS_GPIO_Port, W25Q128_CS_Pin, GPIO_PIN_SET);
}

static W25Q128_Status_t SPI_Transmit(const uint8_t *data, uint16_t len)
{
  return (HAL_SPI_Transmit(&hspi1, (uint8_t *)data, len, BUSY_POLL_MS) == HAL_OK)
             ? W25Q128_OK : W25Q128_ERROR;
}

static W25Q128_Status_t SPI_Receive(uint8_t *data, uint16_t len)
{
  return (HAL_SPI_Receive(&hspi1, data, len, BUSY_POLL_MS) == HAL_OK)
             ? W25Q128_OK : W25Q128_ERROR;
}

static W25Q128_Status_t SPI_TransmitReceive(uint8_t *rx, const uint8_t *tx, uint16_t len)
{
  return (HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, BUSY_POLL_MS) == HAL_OK)
             ? W25Q128_OK : W25Q128_ERROR;
}

/* Send command byte + 24-bit address (MSB first) */
static W25Q128_Status_t SPI_SendCmdAddr(uint8_t cmd, uint32_t addr)
{
  uint8_t buf[4];
  buf[0] = cmd;
  buf[1] = (uint8_t)(addr >> 16);
  buf[2] = (uint8_t)(addr >> 8);
  buf[3] = (uint8_t)(addr);
  return SPI_Transmit(buf, 4);
}

/* -------------------------------------------------------------------------- */
/* Internal operations                                                        */
/* -------------------------------------------------------------------------- */
static W25Q128_Status_t WriteEnable(void)
{
  CS_Low();
  uint8_t cmd = CMD_WRITE_ENABLE;
  W25Q128_Status_t st = SPI_Transmit(&cmd, 1);
  CS_High();
  return st;
}

/* Read N raw bytes after sending cmd (+ optional 24-bit addr) */
static W25Q128_Status_t ReadWithCmd(uint8_t cmd, uint8_t dummy_cnt,
                                    uint8_t *out, uint16_t len)
{
  uint8_t dummy = DUMMY_BYTE;
  CS_Low();
  if (SPI_Transmit(&cmd, 1) != W25Q128_OK)
  {
    CS_High();
    return W25Q128_ERROR;
  }
  for (uint8_t i = 0; i < dummy_cnt; i++)
  {
    if (SPI_Transmit(&dummy, 1) != W25Q128_OK)
    {
      CS_High();
      return W25Q128_ERROR;
    }
  }
  W25Q128_Status_t st = W25Q128_OK;
  if (out != NULL && len > 0U)
  {
    st = SPI_Receive(out, len);
  }
  CS_High();
  return st;
}

W25Q128_Status_t W25Q128_ReadStatus(uint8_t *sr1)
{
  if (sr1 == NULL)
  {
    return W25Q128_ERROR;
  }
  uint8_t cmd = CMD_READ_STATUS1;
  uint8_t rx = 0;
  CS_Low();
  if (SPI_Transmit(&cmd, 1) != W25Q128_OK ||
      SPI_Receive(&rx, 1) != W25Q128_OK)
  {
    CS_High();
    return W25Q128_ERROR;
  }
  CS_High();
  *sr1 = rx;
  return W25Q128_OK;
}

W25Q128_Status_t W25Q128_WaitBusy(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  uint8_t sr1 = 0;
  do
  {
    if (W25Q128_ReadStatus(&sr1) != W25Q128_OK)
    {
      return W25Q128_ERROR;
    }
    if ((sr1 & W25Q128_SR1_BUSY) == 0U)
    {
      return W25Q128_OK;
    }
  } while ((HAL_GetTick() - start) < timeout_ms);
  return W25Q128_TIMEOUT;
}

static W25Q128_Status_t EraseCmd(uint8_t cmd, uint32_t addr)
{
  if (WriteEnable() != W25Q128_OK)
  {
    return W25Q128_ERROR;
  }
  CS_Low();
  W25Q128_Status_t st = SPI_SendCmdAddr(cmd, addr);
  CS_High();
  if (st != W25Q128_OK)
  {
    return st;
  }
  return W25Q128_WaitBusy(BUSY_POLL_MS * 4U);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */
W25Q128_Status_t W25Q128_Init(void)
{
  /* CS idle high */
  CS_High();

  /* Release deep power-down and read JEDEC ID to verify the chip */
  uint32_t id = 0;
  if (W25Q128_ReadID(&id) != W25Q128_OK)
  {
    return W25Q128_ERROR;
  }
  return (id == W25Q128_JEDEC_ID) ? W25Q128_OK : W25Q128_BAD_ID;
}

W25Q128_Status_t W25Q128_ReadID(uint32_t *id)
{
  if (id == NULL)
  {
    return W25Q128_ERROR;
  }
  uint8_t cmd = CMD_JEDEC_ID;
  uint8_t rx[3] = {0};
  CS_Low();
  if (SPI_Transmit(&cmd, 1) != W25Q128_OK ||
      SPI_Receive(rx, 3) != W25Q128_OK)
  {
    CS_High();
    return W25Q128_ERROR;
  }
  CS_High();
  *id = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | rx[2];
  return W25Q128_OK;
}

W25Q128_Status_t W25Q128_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
  if (buf == NULL || len == 0U || (addr + len) > W25Q128_FLASH_SIZE)
  {
    return W25Q128_ERROR;
  }
  if (W25Q128_WaitBusy(BUSY_POLL_MS) != W25Q128_OK)
  {
    return W25Q128_TIMEOUT;
  }
  CS_Low();
  W25Q128_Status_t st = SPI_SendCmdAddr(CMD_READ_DATA, addr);
  if (st == W25Q128_OK)
  {
    st = SPI_Receive(buf, (uint16_t)len);
  }
  CS_High();
  return st;
}

W25Q128_Status_t W25Q128_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
  if (buf == NULL || len == 0U || (addr + len) > W25Q128_FLASH_SIZE)
  {
    return W25Q128_ERROR;
  }

  uint32_t remain = len;
  uint32_t cur = addr;
  const uint8_t *p = buf;

  while (remain > 0U)
  {
    /* Bytes until the end of the current 256B page (no page wrap allowed) */
    uint32_t page_off = cur % W25Q128_PAGE_SIZE;
    uint32_t chunk = W25Q128_PAGE_SIZE - page_off;
    if (chunk > remain)
    {
      chunk = remain;
    }

    if (W25Q128_WaitBusy(BUSY_POLL_MS) != W25Q128_OK)
    {
      return W25Q128_TIMEOUT;
    }
    if (WriteEnable() != W25Q128_OK)
    {
      return W25Q128_ERROR;
    }

    CS_Low();
    W25Q128_Status_t st = SPI_SendCmdAddr(CMD_PAGE_PROGRAM, cur);
    if (st == W25Q128_OK)
    {
      st = SPI_Transmit(p, (uint16_t)chunk);
    }
    CS_High();
    if (st != W25Q128_OK)
    {
      return st;
    }
    if (W25Q128_WaitBusy(BUSY_POLL_MS) != W25Q128_OK)
    {
      return W25Q128_TIMEOUT;
    }

    cur += chunk;
    p += chunk;
    remain -= chunk;
  }
  return W25Q128_OK;
}

W25Q128_Status_t W25Q128_EraseSector(uint32_t addr)
{
  return EraseCmd(CMD_SECTOR_ERASE_4K, addr);
}

W25Q128_Status_t W25Q128_EraseBlock32K(uint32_t addr)
{
  return EraseCmd(CMD_BLOCK_ERASE_32K, addr);
}

W25Q128_Status_t W25Q128_EraseBlock64K(uint32_t addr)
{
  return EraseCmd(CMD_BLOCK_ERASE_64K, addr);
}

W25Q128_Status_t W25Q128_EraseChip(void)
{
  if (WriteEnable() != W25Q128_OK)
  {
    return W25Q128_ERROR;
  }
  CS_Low();
  uint8_t cmd = CMD_CHIP_ERASE;
  W25Q128_Status_t st = SPI_Transmit(&cmd, 1);
  CS_High();
  if (st != W25Q128_OK)
  {
    return st;
  }
  /* Full chip erase may take up to ~200s */
  return W25Q128_WaitBusy(200000U);
}

W25Q128_Status_t W25Q128_ReadStatus2(uint8_t *sr2)
{
  if (sr2 == NULL)
  {
    return W25Q128_ERROR;
  }
  uint8_t cmd = CMD_READ_STATUS2;
  uint8_t rx = 0;
  CS_Low();
  if (SPI_Transmit(&cmd, 1) != W25Q128_OK || SPI_Receive(&rx, 1) != W25Q128_OK)
  {
    CS_High();
    return W25Q128_ERROR;
  }
  CS_High();
  *sr2 = rx;
  return W25Q128_OK;
}

W25Q128_Status_t W25Q128_WriteEnable(void)
{
  return WriteEnable();
}

W25Q128_Status_t W25Q128_WriteDisable(void)
{
  CS_Low();
  uint8_t cmd = CMD_WRITE_DISABLE;
  W25Q128_Status_t st = SPI_Transmit(&cmd, 1);
  CS_High();
  return st;
}

W25Q128_Status_t W25Q128_ReadUniqueID(uint8_t uid[8])
{
  if (uid == NULL)
  {
    return W25Q128_ERROR;
  }
  /* 0x4B + 4 dummy bytes + 8 bytes unique ID */
  return ReadWithCmd(CMD_UNIQUE_ID, 4, uid, 8);
}

W25Q128_Status_t W25Q128_PowerDown(void)
{
  CS_Low();
  uint8_t cmd = CMD_POWER_DOWN;
  W25Q128_Status_t st = SPI_Transmit(&cmd, 1);
  CS_High();
  return st;
}

W25Q128_Status_t W25Q128_ReleasePowerDown(void)
{
  /* Release takes ~3us; wait busy afterwards as a simple guard */
  W25Q128_Status_t st = ReadWithCmd(CMD_RELEASE_POWERDOWN, 3, NULL, 0);
  HAL_Delay(1);
  return st;
}

W25Q128_Status_t W25Q128_LockBlock(uint32_t addr)
{
  if (WriteEnable() != W25Q128_OK)
  {
    return W25Q128_ERROR;
  }
  CS_Low();
  W25Q128_Status_t st = SPI_SendCmdAddr(CMD_INDIVIDUAL_LOCK, addr);
  CS_High();
  return st;
}

W25Q128_Status_t W25Q128_UnlockBlock(uint32_t addr)
{
  if (WriteEnable() != W25Q128_OK)
  {
    return W25Q128_ERROR;
  }
  CS_Low();
  W25Q128_Status_t st = SPI_SendCmdAddr(CMD_INDIVIDUAL_UNLOCK, addr);
  CS_High();
  return st;
}

W25Q128_Status_t W25Q128_ReadBlockLock(uint32_t addr, uint8_t *locked)
{
  if (locked == NULL)
  {
    return W25Q128_ERROR;
  }
  uint8_t rx = 0;
  CS_Low();
  if (SPI_SendCmdAddr(CMD_READ_BLOCK_LOCK, addr) != W25Q128_OK ||
      SPI_Receive(&rx, 1) != W25Q128_OK)
  {
    CS_High();
    return W25Q128_ERROR;
  }
  CS_High();
  *locked = (rx & 0x01U);
  return W25Q128_OK;
}

W25Q128_Status_t W25Q128_WriteStatusReg(uint8_t sr1)
{
  if (WriteEnable() != W25Q128_OK)
  {
    return W25Q128_ERROR;
  }
  uint8_t buf[2] = {CMD_WRITE_STATUS, sr1};
  CS_Low();
  W25Q128_Status_t st = SPI_Transmit(buf, 2);
  CS_High();
  if (st != W25Q128_OK)
  {
    return st;
  }
  /* Status register write cycle: ~15ms typical */
  return W25Q128_WaitBusy(100U);
}
