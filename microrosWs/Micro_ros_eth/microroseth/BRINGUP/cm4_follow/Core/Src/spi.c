#include "spi.h"
#include "shared_data.h"
#include "tim.h"

SPI_HandleTypeDef hspi1;

void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;

  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
  /* Ensure SPI1 kernel clock is APB2 (pclk2 = 000) in case CM7 changed it */
  RCC->D2CCIP1R = (RCC->D2CCIP1R & ~RCC_D2CCIP1R_SPI123SEL);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (hspi->Instance == SPI1) {
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin       = GPIO_PIN_5;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

int SPI1_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
  /* Force CFG2: MASTER + SSM (soft NSS, prevents MODF) + CPOL/CPHA + AFCNTR */
  SPI1->CFG2 = SPI_CFG2_MASTER | SPI_CFG2_SSM |
               SPI_CFG2_CPOL  | SPI_CFG2_CPHA |
               SPI_CFG2_AFCNTR;
  SPI1->CR1 |= SPI_CR1_SSI;
  SPI1->IER  = 0;
  SPI1->IFCR = 0xFFFFFFFFU;
  SPI1->CR2  = (SPI1->CR2 & ~SPI_CR2_TSIZE) | ((uint32_t)len << SPI_CR2_TSIZE_Pos);
  /* Enable SPE first, then assert CSTART in a second write.
   * H7 requires SPE already active before CSTART takes effect. */
  SPI1->CR1 = (SPI1->CR1 & ~(SPI_CR1_SPE | SPI_CR1_CSTART)) | SPI_CR1_SPE;
  SPI1->CR1 |= SPI_CR1_CSTART;

  uint16_t tx_i = 0, rx_i = 0;
  uint32_t t0 = Timer2_NowUs();
  while (tx_i < len || rx_i < len) {
    if ((SPI1->SR & SPI_SR_TXP) && tx_i < len) {
      *((__IO uint8_t *)&SPI1->TXDR) = tx[tx_i++];
    }
    if ((SPI1->SR & SPI_SR_RXP) && rx_i < len) {
      rx[rx_i++] = (uint8_t)(*((__IO uint8_t *)&SPI1->RXDR));
    }
    if ((int32_t)(Timer2_NowUs() - t0) > 100000) {
      SHARED_DATA->last_fault_code = 0xDDDD;
      goto fail;
    }
  }

  t0 = Timer2_NowUs();
  while ((SPI1->SR & SPI_SR_EOT) == 0) {
    if ((int32_t)(Timer2_NowUs() - t0) > 100000) {
      SHARED_DATA->last_fault_code = 0xEEEE;
      goto fail;
    }
  }

  SPI1->IFCR = SPI_IFCR_EOTC;
  SPI1->CR1 &= ~SPI_CR1_SPE;
  return 0;

fail:
  SPI1->CR1 &= ~SPI_CR1_SPE;
  SPI1->IFCR = 0xFFFFFFFFU;
  return -1;
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1) {
    __HAL_RCC_SPI1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5 | GPIO_PIN_6);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_5);
  }
}
