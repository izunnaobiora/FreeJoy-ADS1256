#ifndef ADS1256_H
#define ADS1256_H

#include <stdint.h>
#include "stm32f10x.h"

// ADS1256 Register Map
#define ADS1256_REG_STATUS  0x00
#define ADS1256_REG_MUX     0x01
#define ADS1256_REG_ADCON   0x02
#define ADS1256_REG_DRATE   0x03

// ADS1256 SPI Commands
#define ADS1256_CMD_WAKEUP  0x00
#define ADS1256_CMD_RDATA   0x01
#define ADS1256_CMD_SDATAC  0x0F
#define ADS1256_CMD_WREG    0x50
#define ADS1256_CMD_SYNC    0xFC
#define ADS1256_CMD_RESET   0xFE

// Differential Mux Channels
#define ADS1256_MUX_DIFF_0_1  0x01  // Pitch: AIN0 (+) and AIN1 (-)
#define ADS1256_MUX_DIFF_2_3  0x23  // Roll:  AIN2 (+) and AIN3 (-)

// Gain & Data Rate Settings
#define ADS1256_PGA_64        0x06  // 64x Internal Instrumentation Amp (±78.125mV range)
#define ADS1256_DRATE_3750SPS 0xA1  // 3750 Samples Per Second (Fast settling for 1000Hz USB)

// Digital scaling shift (8 = full range, 5 = 8x extra digital boost for rigid load cells)
#define ADS1256_BIT_SHIFT     5

// Pin Definitions: CS on PB0, DRDY on PB1 (Adjust if needed)
#define ADS1256_CS_PORT       GPIOB
#define ADS1256_CS_PIN        GPIO_Pin_0
#define ADS1256_DRDY_PORT     GPIOB
#define ADS1256_DRDY_PIN      GPIO_Pin_1

void ADS1256_Init(void);
void ADS1256_StepPoll(void);
int16_t ADS1256_GetAxisData(uint8_t channel);

#endif // ADS1256_H