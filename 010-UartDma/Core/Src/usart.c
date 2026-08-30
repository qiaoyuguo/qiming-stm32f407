/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"

/* USER CODE BEGIN 0 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

uint8_t           uart_rx_buf[UART_RX_BUF_SIZE];
volatile uint8_t  uart_rx_flag = 0;
volatile uint16_t uart_rx_len  = 0;

static uint8_t    uart_tx_buf[UART_TX_BUF_SIZE];
static volatile uint8_t uart_tx_busy = 0;

/* 启动 UART5 空闲中断 + DMA 接收（HAL 会同时开启 IDLE 中断与 DMA 传输完成中断） */
void uart5_start_rx_dma(void)
{
  HAL_UARTEx_ReceiveToIdle_DMA(&huart5, uart_rx_buf, UART_RX_BUF_SIZE);
  /* 关闭 DMA 半传输中断，只在整帧/空闲时回调 */
  __HAL_DMA_DISABLE_IT(huart5.hdmarx, DMA_IT_HT);
}

/* 通用 DMA 发送：等待上一次发送完成后启动新发送 */
void uart_dma_transmit(const uint8_t *buf, uint16_t len)
{
  if (len == 0) return;
  while (uart_tx_busy) { }   /* 等待上一次 DMA 发送完成 */

  uart_tx_busy = 1;
  if (HAL_UART_Transmit_DMA(&huart5, (uint8_t *)buf, len) != HAL_OK)
  {
    uart_tx_busy = 0;
  }
}

/* printf 风格 DMA 发送 */
int uart_dma_printf(const char *fmt, ...)
{
  va_list args;
  int len;

  va_start(args, fmt);
  len = vsnprintf((char *)uart_tx_buf, UART_TX_BUF_SIZE, fmt, args);
  va_end(args);

  if (len < 0) return -1;
  if (len > (int)UART_TX_BUF_SIZE) len = UART_TX_BUF_SIZE;

  uart_dma_transmit(uart_tx_buf, (uint16_t)len);
  return len;
}

/* 空闲中断 / DMA 接收完成回调：HAL_UARTEx_ReceiveToIdle_DMA 使用 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == UART5)
  {
    uart_rx_len  = Size;
    uart_rx_flag = 1;
    /* 重新启动接收（DMA_NORMAL 模式下传输完成后需重启） */
    uart5_start_rx_dma();
  }
}

/* DMA 发送完成回调：释放发送缓冲 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART5)
  {
    uart_tx_busy = 0;
  }
}

/* 错误回调：重启接收，保证 RX 链路自恢复 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART5)
  {
    __HAL_UART_CLEAR_OREFLAG(huart);
    uart5_start_rx_dma();
  }
}

/* USER CODE END 0 */

UART_HandleTypeDef huart5;
DMA_HandleTypeDef hdma_uart5_rx;
DMA_HandleTypeDef hdma_uart5_tx;

/* UART5 init function */
void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==UART5)
  {
  /* USER CODE BEGIN UART5_MspInit 0 */

  /* USER CODE END UART5_MspInit 0 */
    /* UART5 clock enable */
    __HAL_RCC_UART5_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**UART5 GPIO Configuration
    PC12     ------> UART5_TX
    PD2     ------> UART5_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* UART5 DMA Init */
    /* UART5_RX Init */
    hdma_uart5_rx.Instance = DMA1_Stream0;
    hdma_uart5_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_uart5_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_uart5_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_uart5_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_uart5_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_uart5_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_uart5_rx.Init.Mode = DMA_NORMAL;
    hdma_uart5_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_uart5_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_uart5_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_uart5_rx);

    /* UART5_TX Init */
    hdma_uart5_tx.Instance = DMA1_Stream7;
    hdma_uart5_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_uart5_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_uart5_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_uart5_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_uart5_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_uart5_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_uart5_tx.Init.Mode = DMA_NORMAL;
    hdma_uart5_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_uart5_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_uart5_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_uart5_tx);

    /* UART5 interrupt Init */
    HAL_NVIC_SetPriority(UART5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(UART5_IRQn);
  /* USER CODE BEGIN UART5_MspInit 1 */

  /* USER CODE END UART5_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==UART5)
  {
  /* USER CODE BEGIN UART5_MspDeInit 0 */

  /* USER CODE END UART5_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_UART5_CLK_DISABLE();

    /**UART5 GPIO Configuration
    PC12     ------> UART5_TX
    PD2     ------> UART5_RX
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_12);

    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);

    /* UART5 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);
    HAL_DMA_DeInit(uartHandle->hdmatx);

    /* UART5 interrupt Deinit */
    HAL_NVIC_DisableIRQ(UART5_IRQn);
  /* USER CODE BEGIN UART5_MspDeInit 1 */

  /* USER CODE END UART5_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

