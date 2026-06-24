#ifndef SPI_H
#define SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern SPI_HandleTypeDef hspi1;

void MX_SPI1_Init(void);

int SPI1_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
