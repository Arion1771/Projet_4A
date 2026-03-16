#include "platform_port.h"

#include <stddef.h>
#include <string.h>

#define PERIPH_BASE                0x40000000UL
#define APB2PERIPH_BASE            (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE             (PERIPH_BASE + 0x00020000UL)
#define GPIOA_BASE                 (AHBPERIPH_BASE + 0x0000UL)
#define GPIOB_BASE                 (AHBPERIPH_BASE + 0x0400UL)
#define RCC_BASE                   (AHBPERIPH_BASE + 0x3800UL)
#define SPI1_BASE                  (APB2PERIPH_BASE + 0x3000UL)

#define LED1_PIN                   (1UL << 0)   /* PA0  */
#define LED2_PIN                   (1UL << 3)   /* PB3  */

#define RCC_AHBENR_GPIOAEN         (1UL << 0)
#define RCC_AHBENR_GPIOBEN         (1UL << 1)
#define RCC_APB2ENR_SPI1EN         (1UL << 12)

#define GPIO_AF_SPI1               5UL

#define SPI_CR1_CPHA               (1UL << 0)
#define SPI_CR1_CPOL               (1UL << 1)
#define SPI_CR1_MSTR               (1UL << 2)
#define SPI_CR1_BR_DIV16           (3UL << 3)
#define SPI_CR1_SPE                (1UL << 6)
#define SPI_CR1_SSI                (1UL << 8)
#define SPI_CR1_SSM                (1UL << 9)

#define SPI_SR_RXNE                (1UL << 0)
#define SPI_SR_TXE                 (1UL << 1)

#define SX127X_REG_FIFO            0x00U
#define SX127X_REG_OPMODE          0x01U
#define SX127X_REG_FR_MSB          0x06U
#define SX127X_REG_FR_MID          0x07U
#define SX127X_REG_FR_LSB          0x08U
#define SX127X_REG_PA_CONFIG       0x09U
#define SX127X_REG_LNA             0x0CU
#define SX127X_REG_FIFO_ADDR_PTR   0x0DU
#define SX127X_REG_FIFO_TX_BASE    0x0EU
#define SX127X_REG_FIFO_RX_BASE    0x0FU
#define SX127X_REG_FIFO_RX_CURR    0x10U
#define SX127X_REG_IRQ_FLAGS       0x12U
#define SX127X_REG_RX_NB_BYTES     0x13U
#define SX127X_REG_PKT_SNR_VALUE   0x19U
#define SX127X_REG_PKT_RSSI_VALUE  0x1AU
#define SX127X_REG_MODEM_CFG1      0x1DU
#define SX127X_REG_MODEM_CFG2      0x1EU
#define SX127X_REG_SYMB_TIMEOUT    0x1FU
#define SX127X_REG_PREAMBLE_MSB    0x20U
#define SX127X_REG_PREAMBLE_LSB    0x21U
#define SX127X_REG_PAYLOAD_LEN     0x22U
#define SX127X_REG_MAX_PAYLOAD     0x23U
#define SX127X_REG_HOP_PERIOD      0x24U
#define SX127X_REG_MODEM_CFG3      0x26U
#define SX127X_REG_SYNC_WORD       0x39U
#define SX127X_REG_DIO_MAPPING1    0x40U
#define SX127X_REG_VERSION         0x42U
#define SX127X_REG_PA_DAC          0x4DU

#define SX127X_VERSION_SX1276      0x12U
#define SX127X_VERSION_SX1272      0x22U

#define SX127X_OPMODE_LONG_RANGE   0x80U
#define SX127X_OPMODE_SLEEP        0x00U
#define SX127X_OPMODE_STDBY        0x01U
#define SX127X_OPMODE_TX           0x03U
#define SX127X_OPMODE_RX_CONT      0x05U

#define SX127X_IRQ_RX_TIMEOUT      (1U << 7)
#define SX127X_IRQ_RX_DONE         (1U << 6)
#define SX127X_IRQ_PAYLOAD_CRC_ERR (1U << 5)
#define SX127X_IRQ_TX_DONE         (1U << 3)

#define LORA_RF_HZ                 868000000UL
#define LORA_SYNC_WORD_PRIVATE     0x12U
#define LORA_RX_MAX_PAYLOAD        255U
#define LORA_TX_TIMEOUT_MS         2200U

#define PIN_NONE                   0xFFU

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t ICSCR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHBRSTR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t AHBLPENR;
    volatile uint32_t APB2LPENR;
    volatile uint32_t APB1LPENR;
    volatile uint32_t CSR;
} RCC_TypeDef;

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t CRCPR;
    volatile uint32_t RXCRCR;
    volatile uint32_t TXCRCR;
    volatile uint32_t I2SCFGR;
    volatile uint32_t I2SPR;
} SPI_TypeDef;

typedef struct {
    GPIO_TypeDef *sck_port;
    uint8_t sck_pin;
    GPIO_TypeDef *miso_port;
    uint8_t miso_pin;
    GPIO_TypeDef *mosi_port;
    uint8_t mosi_pin;
    GPIO_TypeDef *nss_port;
    uint8_t nss_pin;
    GPIO_TypeDef *rst_port;
    uint8_t rst_pin;
    GPIO_TypeDef *rf_tx_port;
    uint8_t rf_tx_pin;
    GPIO_TypeDef *rf_rx_port;
    uint8_t rf_rx_pin;
} radio_profile_t;

typedef enum {
    RADIO_STATE_OFF = 0,
    RADIO_STATE_RX,
    RADIO_STATE_TX
} radio_state_t;

#define GPIOA ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB ((GPIO_TypeDef *)GPIOB_BASE)
#define RCC   ((RCC_TypeDef *)RCC_BASE)
#define SPI1  ((SPI_TypeDef *)SPI1_BASE)

static const radio_profile_t s_profiles[] = {
    /* Common SPI1 remap: PB3/PB4/PB5 + NSS on PA15 */
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 15U, GPIOA, 12U, GPIOB, 9U, GPIOB, 8U},
    /* Same bus, NSS on PA4 */
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 4U, GPIOA, 12U, GPIOB, 9U, GPIOB, 8U},
    /* Alternate SPI1 pins on port A */
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOA, 4U, GPIOA, 12U, GPIOB, 9U, GPIOB, 8U},
    /* Alternate SPI1 pins on port A with NSS on PA15 */
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOA, 15U, GPIOA, 12U, GPIOB, 9U, GPIOB, 8U},
    /* Same as profile 0 but reset moved to PA11 */
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 15U, GPIOA, 11U, GPIOB, 9U, GPIOB, 8U},
};

static uint32_t s_now_ms = 0U;
static led_state_t s_led_state = LED_OFF;
static platform_radio_rx_cb_t s_rx_cb = NULL;
static const radio_profile_t *s_profile = NULL;
static uint8_t s_profile_index = 0xFFU;
static uint8_t s_radio_version = 0U;
static radio_state_t s_radio_state = RADIO_STATE_OFF;
static uint32_t s_tx_start_ms = 0U;
static uint8_t s_rx_buffer[LORA_RX_MAX_PAYLOAD];
static bool s_led2_enabled = true;

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles-- != 0UL) {
        __asm volatile ("nop");
    }
}

static uint32_t pin_mask(uint8_t pin)
{
    return (uint32_t)1UL << pin;
}

static void gpio_config_output(GPIO_TypeDef *gpio, uint8_t pin)
{
    uint32_t shift = (uint32_t)pin * 2UL;
    gpio->MODER = (gpio->MODER & ~(3UL << shift)) | (1UL << shift);
    gpio->OTYPER &= ~pin_mask(pin);
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(3UL << shift)) | (2UL << shift);
    gpio->PUPDR &= ~(3UL << shift);
}

static void gpio_config_af(GPIO_TypeDef *gpio, uint8_t pin, uint8_t af)
{
    uint32_t shift = (uint32_t)pin * 2UL;
    uint32_t afr_idx = ((uint32_t)pin >> 3U);
    uint32_t afr_shift = ((uint32_t)pin & 0x7UL) * 4UL;

    gpio->MODER = (gpio->MODER & ~(3UL << shift)) | (2UL << shift);
    gpio->OTYPER &= ~pin_mask(pin);
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(3UL << shift)) | (3UL << shift);
    gpio->PUPDR &= ~(3UL << shift);
    gpio->AFR[afr_idx] = (gpio->AFR[afr_idx] & ~(0xFUL << afr_shift)) | (((uint32_t)af & 0xFUL) << afr_shift);
}

static void gpio_write(GPIO_TypeDef *gpio, uint8_t pin, bool high)
{
    if (high) {
        gpio->BSRR = pin_mask(pin);
    } else {
        gpio->BSRR = pin_mask(pin) << 16;
    }
}

static void spi1_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    SPI1->CR1 = 0U;
    SPI1->CR2 = 0U;
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_DIV16 | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI1->CR1 |= SPI_CR1_SPE;
}

static uint8_t spi1_xfer(uint8_t out)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0UL) {
    }
    *(volatile uint8_t *)&SPI1->DR = out;
    while ((SPI1->SR & SPI_SR_RXNE) == 0UL) {
    }
    return *(volatile uint8_t *)&SPI1->DR;
}

static void radio_nss(bool high)
{
    if (s_profile == NULL) {
        return;
    }
    gpio_write(s_profile->nss_port, s_profile->nss_pin, high);
}

static void radio_write_reg(uint8_t addr, uint8_t value)
{
    radio_nss(false);
    (void)spi1_xfer((uint8_t)(addr | 0x80U));
    (void)spi1_xfer(value);
    radio_nss(true);
}

static uint8_t radio_read_reg(uint8_t addr)
{
    uint8_t value;
    radio_nss(false);
    (void)spi1_xfer((uint8_t)(addr & 0x7FU));
    value = spi1_xfer(0x00U);
    radio_nss(true);
    return value;
}

static void radio_read_fifo(uint8_t *buffer, uint8_t len)
{
    uint8_t i;
    if ((buffer == NULL) || (len == 0U)) {
        return;
    }
    radio_nss(false);
    (void)spi1_xfer((uint8_t)(SX127X_REG_FIFO & 0x7FU));
    for (i = 0U; i < len; i++) {
        buffer[i] = spi1_xfer(0x00U);
    }
    radio_nss(true);
}

static void radio_write_fifo(const uint8_t *buffer, uint8_t len)
{
    uint8_t i;
    if ((buffer == NULL) || (len == 0U)) {
        return;
    }
    radio_nss(false);
    (void)spi1_xfer((uint8_t)(SX127X_REG_FIFO | 0x80U));
    for (i = 0U; i < len; i++) {
        (void)spi1_xfer(buffer[i]);
    }
    radio_nss(true);
}

static void radio_set_rf_switch(bool tx_on)
{
    if (s_profile == NULL) {
        return;
    }
    /*
     * Wyres W_BASE revC switch policy (from BSP reference):
     * - TX: CTRL1=0, CTRL2=1
     * - RX: CTRL1=1, CTRL2=1
     */
    if (s_profile->rf_tx_pin != PIN_NONE) {
        gpio_write(s_profile->rf_tx_port, s_profile->rf_tx_pin, !tx_on);
    }
    if (s_profile->rf_rx_pin != PIN_NONE) {
        gpio_write(s_profile->rf_rx_port, s_profile->rf_rx_pin, true);
    }
}

static void radio_reset(void)
{
    if ((s_profile == NULL) || (s_profile->rst_pin == PIN_NONE)) {
        return;
    }
    gpio_write(s_profile->rst_port, s_profile->rst_pin, false);
    delay_cycles(24000UL);
    gpio_write(s_profile->rst_port, s_profile->rst_pin, true);
    delay_cycles(80000UL);
}

static void radio_apply_profile(const radio_profile_t *profile)
{
    s_profile = profile;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;

    gpio_config_af(profile->sck_port, profile->sck_pin, GPIO_AF_SPI1);
    gpio_config_af(profile->miso_port, profile->miso_pin, GPIO_AF_SPI1);
    gpio_config_af(profile->mosi_port, profile->mosi_pin, GPIO_AF_SPI1);

    gpio_config_output(profile->nss_port, profile->nss_pin);
    gpio_write(profile->nss_port, profile->nss_pin, true);

    if (profile->rst_pin != PIN_NONE) {
        gpio_config_output(profile->rst_port, profile->rst_pin);
        gpio_write(profile->rst_port, profile->rst_pin, true);
    }

    if (profile->rf_tx_pin != PIN_NONE) {
        gpio_config_output(profile->rf_tx_port, profile->rf_tx_pin);
        gpio_write(profile->rf_tx_port, profile->rf_tx_pin, false);
    }

    if (profile->rf_rx_pin != PIN_NONE) {
        gpio_config_output(profile->rf_rx_port, profile->rf_rx_pin);
        gpio_write(profile->rf_rx_port, profile->rf_rx_pin, false);
    }

    spi1_init();
}

static bool radio_detect(void)
{
    uint8_t idx;
    uint8_t version;

    s_profile = NULL;
    s_profile_index = 0xFFU;
    s_radio_version = 0U;

    for (idx = 0U; idx < (uint8_t)(sizeof(s_profiles) / sizeof(s_profiles[0])); idx++) {
        radio_apply_profile(&s_profiles[idx]);
        radio_reset();

        version = radio_read_reg(SX127X_REG_VERSION);
        if ((version != SX127X_VERSION_SX1272) && (version != SX127X_VERSION_SX1276)) {
            delay_cycles(5000UL);
            version = radio_read_reg(SX127X_REG_VERSION);
        }

        if ((version == SX127X_VERSION_SX1272) || (version == SX127X_VERSION_SX1276)) {
            s_profile_index = idx;
            s_radio_version = version;
            s_led2_enabled = !((s_profiles[idx].sck_port == GPIOB) && (s_profiles[idx].sck_pin == 3U));
            return true;
        }
    }

    return false;
}

static void radio_configure_lora(void)
{
    uint64_t frf;
    uint8_t lna;

    radio_write_reg(SX127X_REG_OPMODE, SX127X_OPMODE_LONG_RANGE | SX127X_OPMODE_SLEEP);
    delay_cycles(8000UL);
    radio_write_reg(SX127X_REG_OPMODE, SX127X_OPMODE_LONG_RANGE | SX127X_OPMODE_STDBY);

    frf = (((uint64_t)LORA_RF_HZ) << 19) / 32000000ULL;
    radio_write_reg(SX127X_REG_FR_MSB, (uint8_t)(frf >> 16));
    radio_write_reg(SX127X_REG_FR_MID, (uint8_t)(frf >> 8));
    radio_write_reg(SX127X_REG_FR_LSB, (uint8_t)(frf));

    radio_write_reg(SX127X_REG_PA_CONFIG, 0x8CU); /* PA_BOOST, ~14 dBm */

    lna = radio_read_reg(SX127X_REG_LNA);
    radio_write_reg(SX127X_REG_LNA, (uint8_t)(lna | 0x03U));

    radio_write_reg(SX127X_REG_FIFO_TX_BASE, 0x00U);
    radio_write_reg(SX127X_REG_FIFO_RX_BASE, 0x00U);
    radio_write_reg(SX127X_REG_FIFO_ADDR_PTR, 0x00U);

    if (s_radio_version == SX127X_VERSION_SX1272) {
        /* BW125 / CR4:5 / explicit / CRC on / low data rate off */
        radio_write_reg(SX127X_REG_MODEM_CFG1, 0x0AU);
        /* SF7 / AGC on / symb timeout msb = 0 */
        radio_write_reg(SX127X_REG_MODEM_CFG2, 0x74U);
    } else {
        /* BW125 / CR4:5 / explicit */
        radio_write_reg(SX127X_REG_MODEM_CFG1, 0x72U);
        /* SF7 / CRC on / symb timeout msb = 0 */
        radio_write_reg(SX127X_REG_MODEM_CFG2, 0x74U);
        /* AGC on */
        radio_write_reg(SX127X_REG_MODEM_CFG3, 0x04U);
        radio_write_reg(SX127X_REG_PA_DAC, 0x84U);
    }

    radio_write_reg(SX127X_REG_SYMB_TIMEOUT, 0x05U);
    radio_write_reg(SX127X_REG_PREAMBLE_MSB, 0x00U);
    radio_write_reg(SX127X_REG_PREAMBLE_LSB, 0x08U);
    radio_write_reg(SX127X_REG_PAYLOAD_LEN, 0x40U);
    radio_write_reg(SX127X_REG_MAX_PAYLOAD, 0x40U);
    radio_write_reg(SX127X_REG_HOP_PERIOD, 0x00U);
    radio_write_reg(SX127X_REG_SYNC_WORD, LORA_SYNC_WORD_PRIVATE);
    radio_write_reg(SX127X_REG_DIO_MAPPING1, 0x00U);
    radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
}

static void radio_start_rx_continuous(void)
{
    radio_set_rf_switch(false);
    radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
    radio_write_reg(SX127X_REG_FIFO_ADDR_PTR, radio_read_reg(SX127X_REG_FIFO_RX_BASE));
    radio_write_reg(SX127X_REG_OPMODE, SX127X_OPMODE_LONG_RANGE | SX127X_OPMODE_RX_CONT);
    s_radio_state = RADIO_STATE_RX;
}

static bool radio_start_tx(const uint8_t *data, uint16_t len)
{
    uint8_t tx_len;

    if ((data == NULL) || (len == 0U) || (len > 255U)) {
        return false;
    }

    tx_len = (uint8_t)len;

    radio_set_rf_switch(true);
    radio_write_reg(SX127X_REG_OPMODE, SX127X_OPMODE_LONG_RANGE | SX127X_OPMODE_STDBY);
    radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
    radio_write_reg(SX127X_REG_FIFO_ADDR_PTR, radio_read_reg(SX127X_REG_FIFO_TX_BASE));
    radio_write_fifo(data, tx_len);
    radio_write_reg(SX127X_REG_PAYLOAD_LEN, tx_len);
    radio_write_reg(SX127X_REG_OPMODE, SX127X_OPMODE_LONG_RANGE | SX127X_OPMODE_TX);
    s_radio_state = RADIO_STATE_TX;
    s_tx_start_ms = s_now_ms;
    return true;
}

static int8_t radio_read_snr_db(void)
{
    int8_t snr_q4 = (int8_t)radio_read_reg(SX127X_REG_PKT_SNR_VALUE);
    return (int8_t)(snr_q4 / 4);
}

static int16_t radio_read_rssi_dbm(int8_t snr_db)
{
    uint8_t raw = radio_read_reg(SX127X_REG_PKT_RSSI_VALUE);
    int16_t base = (s_radio_version == SX127X_VERSION_SX1272) ? -139 : -157;
    int16_t rssi = (int16_t)raw + base;
    if (snr_db < 0) {
        rssi += snr_db;
    }
    return rssi;
}

void platform_port_set_time_ms(uint32_t now_ms)
{
    s_now_ms = now_ms;
}

uint32_t platform_millis(void)
{
    return s_now_ms;
}

bool platform_radio_init(platform_radio_rx_cb_t cb)
{
    s_rx_cb = cb;
    s_radio_state = RADIO_STATE_OFF;

    if (!radio_detect()) {
        return false;
    }

    radio_configure_lora();
    radio_start_rx_continuous();
    return true;
}

void platform_radio_process(void)
{
    uint8_t irq_flags;

    if ((s_profile == NULL) || (s_radio_state == RADIO_STATE_OFF)) {
        return;
    }

    irq_flags = radio_read_reg(SX127X_REG_IRQ_FLAGS);

    if (s_radio_state == RADIO_STATE_TX) {
        if ((irq_flags & SX127X_IRQ_TX_DONE) != 0U) {
            radio_write_reg(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_TX_DONE);
            radio_start_rx_continuous();
            irq_flags &= (uint8_t)~SX127X_IRQ_TX_DONE;
        } else if ((s_now_ms - s_tx_start_ms) > LORA_TX_TIMEOUT_MS) {
            radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
            radio_start_rx_continuous();
            return;
        }
    }

    if ((irq_flags & SX127X_IRQ_RX_TIMEOUT) != 0U) {
        radio_write_reg(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_RX_TIMEOUT);
    }

    if ((irq_flags & SX127X_IRQ_RX_DONE) != 0U) {
        uint8_t len;
        uint8_t fifo_addr;
        int8_t snr_db;
        int16_t rssi_dbm;

        radio_write_reg(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_RX_DONE | SX127X_IRQ_PAYLOAD_CRC_ERR);
        if ((irq_flags & SX127X_IRQ_PAYLOAD_CRC_ERR) != 0U) {
            return;
        }

        len = radio_read_reg(SX127X_REG_RX_NB_BYTES);
        if (len == 0U) {
            return;
        }
        if (len > LORA_RX_MAX_PAYLOAD) {
            len = LORA_RX_MAX_PAYLOAD;
        }

        fifo_addr = radio_read_reg(SX127X_REG_FIFO_RX_CURR);
        radio_write_reg(SX127X_REG_FIFO_ADDR_PTR, fifo_addr);
        radio_read_fifo(s_rx_buffer, len);

        snr_db = radio_read_snr_db();
        rssi_dbm = radio_read_rssi_dbm(snr_db);

        if (s_rx_cb != NULL) {
            s_rx_cb(LINK_PREDECESSOR, s_rx_buffer, len, rssi_dbm, snr_db);
        }
    }
}

bool platform_radio_send(link_direction_t dir, const uint8_t *data, uint16_t len)
{
    (void)dir;

    if ((s_profile == NULL) || (s_radio_state == RADIO_STATE_OFF) || (s_radio_state == RADIO_STATE_TX)) {
        return false;
    }

    return radio_start_tx(data, len);
}

uint8_t platform_radio_version(void)
{
    return s_radio_version;
}

uint8_t platform_radio_profile(void)
{
    return s_profile_index;
}

void platform_led_set(led_state_t state)
{
    s_led_state = state;

    switch (state) {
    case LED_OFF:
        GPIOA->BSRR = (LED1_PIN << 16);
        if (s_led2_enabled) {
            GPIOB->BSRR = (LED2_PIN << 16);
        }
        break;

    case LED_NEUTRAL:
        GPIOA->BSRR = LED1_PIN;
        if (s_led2_enabled) {
            GPIOB->BSRR = (LED2_PIN << 16);
        }
        break;

    case LED_INSERTED:
        GPIOA->BSRR = (LED1_PIN << 16);
        if (s_led2_enabled) {
            GPIOB->BSRR = LED2_PIN;
        }
        break;

    case LED_ALERT:
    default:
        GPIOA->BSRR = LED1_PIN;
        if (s_led2_enabled) {
            GPIOB->BSRR = LED2_PIN;
        }
        break;
    }
}

led_state_t platform_port_get_led_state(void)
{
    return s_led_state;
}

uint16_t platform_battery_mv(void)
{
    return 3900U;
}
