#include "ads1256.h"

static volatile int16_t ads_axis_data[2] = {0, 0};
static uint8_t current_channel = 0;
static const uint8_t mux_channels[2] = {ADS1256_MUX_DIFF_0_1, ADS1256_MUX_DIFF_2_3};

// Busy-wait microsecond delay at 72MHz (~72 cycles per us)
static void delay_us(uint32_t us) {
    uint32_t count = us * 12;
    while (count--) { __NOP(); }
}

static inline void CS_Low(void)  { ADS1256_CS_PORT->BRR  = ADS1256_CS_PIN; }
static inline void CS_High(void) { ADS1256_CS_PORT->BSRR = ADS1256_CS_PIN; }
static inline uint8_t DRDY_IsLow(void) {
    return ((ADS1256_DRDY_PORT->IDR & ADS1256_DRDY_PIN) == 0);
}

static uint8_t SPI1_Transfer(uint8_t byte) {
    while ((SPI1->SR & SPI_I2S_FLAG_TXE) == 0);
    SPI1->DR = byte;
    while ((SPI1->SR & SPI_I2S_FLAG_RXNE) == 0);
    return (uint8_t)(SPI1->DR);
}

static void ADS1256_WriteReg(uint8_t reg, uint8_t val) {
    SPI1_Transfer(ADS1256_CMD_WREG | (reg & 0x0F));
    SPI1_Transfer(0x00); // Write 1 register (N - 1 = 0)
    SPI1_Transfer(val);
    delay_us(2);
}

static int16_t ADS1256_Scale24To16Bit(int32_t raw24) {
    // Sign-extend 24-bit Two's Complement to 32-bit signed integer
    if (raw24 & 0x800000) {
        raw24 |= 0xFF000000;
    }
    int32_t scaled = raw24 >> ADS1256_BIT_SHIFT;

    // Clamp to 16-bit signed limits for FreeJoy
    if (scaled > 32767)  scaled = 32767;
    if (scaled < -32767) scaled = -32767;
    return (int16_t)scaled;
}

void ADS1256_Init(void) {
    // 1. Enable AFIO, GPIOA, GPIOB, and SPI1 clocks, and ensure SPI1 is on PA5/PA6/PA7
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_SPI1EN;
    AFIO->MAPR   &= ~AFIO_MAPR_SPI1_REMAP;

    // 2. Configure CS (PB0) as Push-Pull Output, DRDY (PB1) as Input Pull-Up
    GPIOB->CRL &= ~(0xFF);
    GPIOB->CRL |=  (0x03 << 0) | (0x08 << 4); // PB0: Out 50MHz, PB1: In Pull-Up
    GPIOB->BSRR = ADS1256_DRDY_PIN | ADS1256_CS_PIN;

    // 3. Configure SPI1 Pins: PA5 (SCK), PA6 (MISO), PA7 (MOSI)
    GPIOA->CRL &= ~(0xFFF00000);
    GPIOA->CRL |=  (0xB4B00000); // PA5/PA7 Alt-Func Push-Pull, PA6 Input Floating

    // 4. Configure SPI1: Mode 1 (CPOL=0, CPHA=1), Master, Prescaler 64 (1.125MHz)
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_CPHA | SPI_CR1_BR_2 | SPI_CR1_BR_0 | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI1->CR1 |= SPI_CR1_SPE;

    delay_us(10000); // Wait 10ms for ADS1256 power-up

    // 5. Reset and Stop Continuous Read Mode
    CS_Low();
    SPI1_Transfer(ADS1256_CMD_RESET);
    delay_us(1000);
    SPI1_Transfer(ADS1256_CMD_SDATAC);
    delay_us(10);

    // 6. Configure STATUS (Buffer OFF, Auto-Cal ON), PGA = 64x, DRATE = 3750 SPS
    ADS1256_WriteReg(ADS1256_REG_STATUS, 0x04);
    ADS1256_WriteReg(ADS1256_REG_ADCON,  ADS1256_PGA_64);
    ADS1256_WriteReg(ADS1256_REG_DRATE,  ADS1256_DRATE_3750SPS);
    ADS1256_WriteReg(ADS1256_REG_MUX,    mux_channels[0]);

    SPI1_Transfer(ADS1256_CMD_SYNC);
    delay_us(5);
    SPI1_Transfer(ADS1256_CMD_WAKEUP);
    CS_High();
}

// Non-blocking pipelined read & channel switch
void ADS1256_StepPoll(void) {
    if (!DRDY_IsLow()) {
        return; // Conversion still in progress; immediately return so USB loop doesn't lag
    }

    uint8_t next_channel = (current_channel + 1) & 0x01;

    CS_Low();

    // Step A: Pipeline the NEXT channel into the MUX register
    ADS1256_WriteReg(ADS1256_REG_MUX, mux_channels[next_channel]);
    SPI1_Transfer(ADS1256_CMD_SYNC);
    delay_us(5);
    SPI1_Transfer(ADS1256_CMD_WAKEUP);
    delay_us(2);

    // Step B: Read the 24-bit result from the CURRENT channel
    SPI1_Transfer(ADS1256_CMD_RDATA);
    delay_us(7); // Wait t6 delay (50 CLK cycles) before clocking out data

    uint32_t b0 = SPI1_Transfer(0xFF);
    uint32_t b1 = SPI1_Transfer(0xFF);
    uint32_t b2 = SPI1_Transfer(0xFF);

    CS_High();

    int32_t raw24 = (int32_t)((b0 << 16) | (b1 << 8) | b2);
    ads_axis_data[current_channel] = ADS1256_Scale24To16Bit(raw24);

    current_channel = next_channel;
}

int16_t ADS1256_GetAxisData(uint8_t channel) {
    return ads_axis_data[channel & 0x01];
}
