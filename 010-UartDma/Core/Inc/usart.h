/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart5;

/* USER CODE BEGIN Private defines */

#define UART_RX_BUF_SIZE  128
#define UART_TX_BUF_SIZE  256

/* 收到一帧数据标志及长度（由 HAL_UARTEx_RxEventCallback 置位） */
extern volatile uint8_t  uart_rx_flag;
extern volatile uint16_t uart_rx_len;
extern uint8_t           uart_rx_buf[UART_RX_BUF_SIZE];

/* 通过 UART5 + DMA 发送（printf 风格，返回格式化后的长度，失败返回负值） */
int uart_dma_printf(const char *fmt, ...);

/* 通过 UART5 + DMA 发送任意缓冲区（内部等待上一次发送完成） */
void uart_dma_transmit(const uint8_t *buf, uint16_t len);

/* 启动 UART5 空闲中断 + DMA 接收 */
void uart5_start_rx_dma(void);

/* USER CODE END Private defines */

void MX_UART5_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

