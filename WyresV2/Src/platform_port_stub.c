#include "platform_port.h"

#include <stddef.h>
#include <string.h>

#define PERIPH_BASE                0x40000000UL
#define APB2PERIPH_BASE            (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE             (PERIPH_BASE + 0x00020000UL)
#define GPIOA_BASE                 (AHBPERIPH_BASE + 0x0000UL)
#define GPIOB_BASE                 (AHBPERIPH_BASE + 0x0400UL)
#define GPIOC_BASE                 (AHBPERIPH_BASE + 0x0800UL)
#define RCC_BASE                   (AHBPERIPH_BASE + 0x3800UL)
#define AFIO_BASE                  (APB2PERIPH_BASE + 0x0000UL)
#define SPI1_BASE                  (APB2PERIPH_BASE + 0x3000UL)

#define LED1_PIN                   (1UL << 0)   /* PA0  */
#define LED2_PIN                   (1UL << 3)   /* PB3  */

#define RCC_AHBENR_GPIOAEN         (1UL << 0)
#define RCC_AHBENR_GPIOBEN         (1UL << 1)
#define RCC_AHBENR_GPIOCEN         (1UL << 2)
#define RCC_APB2ENR_SPI1EN         (1UL << 12)
#define RCC_APB2ENR_AFIOEN         (1UL << 0)

#define AFIO_MAPR_SWJ_CFG_MASK         (7UL << 24)
#define AFIO_MAPR_SWJ_CFG_JTAGDISABLE  (2UL << 24)

#define GPIO_AF_SPI1               5UL

#define SPI_CR1_CPHA               (1UL << 0)
#define SPI_CR1_CPOL               (1UL << 1)
#define SPI_CR1_MSTR               (1UL << 2)
#define SPI_CR1_BR_DIV16           (3UL << 3)
#define SPI_CR1_BR_DIV64           (5UL << 3)
#define SPI_CR1_BR_DIV256          (7UL << 3)
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
#define LORA_TX_RECOVER_MS         300U
#define LORA_RX_GUARD_MS           250U
#define LORA_RX_SOFT_RECOVER_MS    900U
#define LORA_RX_STUCK_RESET_MS     3500U
#define LORA_RX_RECOVER_GAP_MS     1000U
#define RADIO_PROFILE_PREFERRED    0U
#define RADIO_USE_TCXO             0U
#define RADIO_USE_RF_SWITCH        1U
#define RADIO_FORCE_PROFILE_ONLY   0U
#define RADIO_FORCE_PROFILE_INDEX  26U
#define RADIO_FORCE_SLOW_SPI       1U

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
    volatile uint32_t EVCR;
    volatile uint32_t MAPR;
} AFIO_TypeDef;

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
    GPIO_TypeDef *tcxo_port;
    uint8_t tcxo_pin;
} radio_profile_t;

typedef enum {
    RADIO_STATE_OFF = 0,
    RADIO_STATE_RX,
    RADIO_STATE_TX
} radio_state_t;

#define GPIOA ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC ((GPIO_TypeDef *)GPIOC_BASE)
#define RCC   ((RCC_TypeDef *)RCC_BASE)
#define AFIO  ((AFIO_TypeDef *)AFIO_BASE)
#define SPI1  ((SPI_TypeDef *)SPI1_BASE)

static const radio_profile_t s_profiles[] = {
    /*
     * High-priority profiles inferred from W_BASE V2 REVC schematic:
     * SPI1_CLK->PA5, SPI1_MISO->PA6, SPI1_MOSI->PA7, SPI1_CS->PB0
     * RFSwitch_TX->PA4, RFSwitch_RX likely on PC13, SX1272_RST likely on PA2/PA1.
     */
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOB, 0U, GPIOA, 2U, GPIOA, 4U, GPIOC, 13U, NULL, PIN_NONE},
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOB, 0U, GPIOA, 1U, GPIOA, 4U, GPIOC, 13U, NULL, PIN_NONE},
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOB, 0U, NULL, PIN_NONE, GPIOA, 4U, GPIOC, 13U, NULL, PIN_NONE},
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOB, 0U, GPIOA, 2U, GPIOA, 4U, NULL, PIN_NONE, NULL, PIN_NONE},
    {GPIOA, 5U, GPIOA, 7U, GPIOA, 6U, GPIOB, 0U, GPIOA, 2U, GPIOA, 4U, GPIOC, 13U, NULL, PIN_NONE},
    {GPIOA, 5U, GPIOA, 7U, GPIOA, 6U, GPIOB, 0U, NULL, PIN_NONE, GPIOA, 4U, GPIOC, 13U, NULL, PIN_NONE},

    /*
     * Alternate hypothesis:
     * SPI1_CLK->PB8, SPI1_MISO->PB7, SPI1_MOSI->PB6, SPI1_CS->PB5,
     * RFSwitch_TX->PB9, RFSwitch_RX->PB4
     */
    {GPIOB, 8U, GPIOB, 7U, GPIOB, 6U, GPIOB, 5U, GPIOA, 12U, GPIOB, 9U, GPIOB, 4U, GPIOB, 0U},
    {GPIOB, 8U, GPIOB, 7U, GPIOB, 6U, GPIOB, 5U, GPIOA, 11U, GPIOB, 9U, GPIOB, 4U, GPIOB, 0U},
    {GPIOB, 8U, GPIOB, 7U, GPIOB, 6U, GPIOB, 5U, NULL, PIN_NONE, GPIOB, 9U, GPIOB, 4U, GPIOB, 0U},
    {GPIOB, 8U, GPIOB, 6U, GPIOB, 7U, GPIOB, 5U, GPIOA, 12U, GPIOB, 9U, GPIOB, 4U, GPIOB, 0U},
    {GPIOB, 8U, GPIOB, 6U, GPIOB, 7U, GPIOB, 5U, GPIOA, 11U, GPIOB, 9U, GPIOB, 4U, GPIOB, 0U},
    {GPIOB, 8U, GPIOB, 6U, GPIOB, 7U, GPIOB, 5U, NULL, PIN_NONE, GPIOB, 9U, GPIOB, 4U, GPIOB, 0U},

    {GPIOB, 7U, GPIOB, 6U, GPIOB, 5U, GPIOB, 4U, GPIOA, 12U, GPIOB, 9U, GPIOB, 8U, GPIOB, 0U},
    {GPIOB, 7U, GPIOB, 6U, GPIOB, 5U, GPIOB, 4U, GPIOA, 11U, GPIOB, 9U, GPIOB, 8U, GPIOB, 0U},
    {GPIOB, 7U, GPIOB, 6U, GPIOB, 5U, GPIOB, 4U, NULL, PIN_NONE, GPIOB, 9U, GPIOB, 8U, GPIOB, 0U},
    {GPIOB, 7U, GPIOB, 5U, GPIOB, 6U, GPIOB, 4U, GPIOA, 12U, GPIOB, 9U, GPIOB, 8U, GPIOB, 0U},
    {GPIOB, 7U, GPIOB, 5U, GPIOB, 6U, GPIOB, 4U, GPIOA, 11U, GPIOB, 9U, GPIOB, 8U, GPIOB, 0U},
    {GPIOB, 7U, GPIOB, 5U, GPIOB, 6U, GPIOB, 4U, NULL, PIN_NONE, GPIOB, 9U, GPIOB, 8U, GPIOB, 0U},

    /* SPI=PB3/4/5, NSS=PA15 */
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 15U, GPIOA, 12U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 15U, GPIOA, 11U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 15U, NULL, PIN_NONE, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},

    /* SPI=PB3/4/5, NSS=PA4 */
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 4U, GPIOA, 12U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 4U, GPIOA, 11U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 4U, NULL, PIN_NONE, NULL, PIN_NONE, NULL, PIN_NONE},

    /* SPI=PA5/6/7, NSS=PA4 */
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOA, 4U, GPIOA, 12U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOA, 4U, GPIOA, 11U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOA, 4U, NULL, PIN_NONE, NULL, PIN_NONE, NULL, PIN_NONE},

    /* SPI=PA5/6/7, NSS=PA15 */
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOA, 15U, GPIOA, 12U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOA, 15U, GPIOA, 11U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOA, 5U, GPIOA, 6U, GPIOA, 7U, GPIOA, 15U, NULL, PIN_NONE, NULL, PIN_NONE, NULL, PIN_NONE},

    /* SPI=PB3/4/5 with MISO/MOSI swapped, NSS=PA15 */
    {GPIOB, 3U, GPIOB, 5U, GPIOB, 4U, GPIOA, 15U, GPIOA, 12U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 5U, GPIOB, 4U, GPIOA, 15U, GPIOA, 11U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 5U, GPIOB, 4U, GPIOA, 15U, NULL, PIN_NONE, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},

    /* SPI=PB3/4/5 with MISO/MOSI swapped, NSS=PA4 */
    {GPIOB, 3U, GPIOB, 5U, GPIOB, 4U, GPIOA, 4U, GPIOA, 12U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 5U, GPIOB, 4U, GPIOA, 4U, GPIOA, 11U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 5U, GPIOB, 4U, GPIOA, 4U, NULL, PIN_NONE, NULL, PIN_NONE, NULL, PIN_NONE},

    /* SPI=PA5/6/7 with MISO/MOSI swapped, NSS=PA4 */
    {GPIOA, 5U, GPIOA, 7U, GPIOA, 6U, GPIOA, 4U, GPIOA, 12U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOA, 5U, GPIOA, 7U, GPIOA, 6U, GPIOA, 4U, GPIOA, 11U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOA, 5U, GPIOA, 7U, GPIOA, 6U, GPIOA, 4U, NULL, PIN_NONE, NULL, PIN_NONE, NULL, PIN_NONE},

    /* SPI=PA5/6/7 with MISO/MOSI swapped, NSS=PA15 */
    {GPIOA, 5U, GPIOA, 7U, GPIOA, 6U, GPIOA, 15U, GPIOA, 12U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOA, 5U, GPIOA, 7U, GPIOA, 6U, GPIOA, 15U, GPIOA, 11U, NULL, PIN_NONE, NULL, PIN_NONE, GPIOB, 0U},
    {GPIOA, 5U, GPIOA, 7U, GPIOA, 6U, GPIOA, 15U, NULL, PIN_NONE, NULL, PIN_NONE, NULL, PIN_NONE},

    /* Extra brute-force on active bus SPI=PB3/4/5 */
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 0U, GPIOA, 12U, GPIOA, 4U, GPIOA, 5U, NULL, PIN_NONE},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 0U, GPIOA, 11U, GPIOA, 4U, GPIOA, 5U, NULL, PIN_NONE},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 0U, NULL, PIN_NONE, GPIOA, 4U, GPIOA, 5U, NULL, PIN_NONE},

    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 1U, GPIOA, 12U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 1U, GPIOA, 11U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 1U, NULL, PIN_NONE, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},

    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 6U, GPIOA, 12U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 6U, GPIOA, 11U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 6U, NULL, PIN_NONE, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},

    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 7U, GPIOA, 12U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 7U, GPIOA, 11U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOB, 7U, NULL, PIN_NONE, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},

    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 8U, GPIOA, 12U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 8U, GPIOA, 11U, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
    {GPIOB, 3U, GPIOB, 4U, GPIOB, 5U, GPIOA, 8U, NULL, PIN_NONE, GPIOA, 4U, GPIOA, 5U, GPIOB, 0U},
};

static uint32_t s_now_ms = 0U;
static led_state_t s_led_state = LED_OFF;
static platform_radio_rx_cb_t s_rx_cb = NULL;
static const radio_profile_t *s_profile = NULL;
static uint8_t s_profile_index = 0xFFU;
static uint8_t s_radio_version = 0U;
static uint8_t s_probe_count = 0U;
static uint8_t s_probe_profile[128U];
static uint8_t s_probe_version[128U];
static uint8_t s_probe_af[128U];
static radio_state_t s_radio_state = RADIO_STATE_OFF;
static uint32_t s_tx_start_ms = 0U;
static uint32_t s_rx_guard_ms = 0U;
static uint32_t s_last_rx_event_ms = 0U;
static uint32_t s_last_recover_ms = 0U;
static uint8_t s_rx_buffer[LORA_RX_MAX_PAYLOAD];
static bool s_led2_enabled = true;
static bool s_spi_cpol = false;
static bool s_spi_cpha = false;
static uint32_t s_spi_half_cycles = 10UL;
static uint32_t s_spi_default_br = SPI_CR1_BR_DIV64;
static bool s_spi_default_cpol = false;
static bool s_spi_default_cpha = false;

static void gpio_config_output(GPIO_TypeDef *gpio, uint8_t pin);
static void gpio_write(GPIO_TypeDef *gpio, uint8_t pin, bool high);

static void debug_port_enable_swd_only(void)
{
    /* Free PB3/PB4 by disabling JTAG while keeping SWD active. */
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG_MASK) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
}

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

static void gpio_config_input(GPIO_TypeDef *gpio, uint8_t pin)
{
    uint32_t shift = (uint32_t)pin * 2UL;
    gpio->MODER &= ~(3UL << shift);
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

static bool gpio_read(GPIO_TypeDef *gpio, uint8_t pin)
{
    return ((gpio->IDR & pin_mask(pin)) != 0UL);
}

static void spi1_init_cfg(uint32_t br, bool cpol, bool cpha);

static void spi1_init(void)
{
    spi1_init_cfg(s_spi_default_br, s_spi_default_cpol, s_spi_default_cpha);
}

static void spi1_init_cfg(uint32_t br, bool cpol, bool cpha)
{
    s_spi_cpol = cpol;
    s_spi_cpha = cpha;

    if (br == SPI_CR1_BR_DIV256) {
        s_spi_half_cycles = 24UL;
    } else if (br == SPI_CR1_BR_DIV64) {
        s_spi_half_cycles = 12UL;
    } else {
        s_spi_half_cycles = 8UL;
    }

    if (s_profile != NULL) {
        gpio_write(s_profile->sck_port, s_profile->sck_pin, s_spi_cpol);
    }
}

static uint8_t spi1_xfer(uint8_t out)
{
    uint8_t in = 0U;
    uint8_t bit;
    bool active = !s_spi_cpol;

    for (bit = 0U; bit < 8U; bit++) {
        bool out_bit = ((out & 0x80U) != 0U);
        out <<= 1;
        in <<= 1;

        if (!s_spi_cpha) {
            gpio_write(s_profile->mosi_port, s_profile->mosi_pin, out_bit);
            delay_cycles(s_spi_half_cycles);
            gpio_write(s_profile->sck_port, s_profile->sck_pin, active);
            delay_cycles(s_spi_half_cycles);
            if (gpio_read(s_profile->miso_port, s_profile->miso_pin)) {
                in |= 1U;
            }
            gpio_write(s_profile->sck_port, s_profile->sck_pin, !active);
        } else {
            gpio_write(s_profile->sck_port, s_profile->sck_pin, active);
            delay_cycles(s_spi_half_cycles);
            gpio_write(s_profile->mosi_port, s_profile->mosi_pin, out_bit);
            delay_cycles(s_spi_half_cycles);
            gpio_write(s_profile->sck_port, s_profile->sck_pin, !active);
            if (gpio_read(s_profile->miso_port, s_profile->miso_pin)) {
                in |= 1U;
            }
        }
        delay_cycles(s_spi_half_cycles);
    }

    return in;
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
    delay_cycles(40UL);
    (void)spi1_xfer((uint8_t)(addr | 0x80U));
    (void)spi1_xfer(value);
    delay_cycles(40UL);
    radio_nss(true);
}

static uint8_t radio_read_reg(uint8_t addr)
{
    uint8_t value;
    radio_nss(false);
    delay_cycles(40UL);
    (void)spi1_xfer((uint8_t)(addr & 0x7FU));
    value = spi1_xfer(0x00U);
    delay_cycles(40UL);
    radio_nss(true);
    return value;
}

static bool radio_rw_check(void)
{
    uint8_t old_mode;
    uint8_t test_mode;
    uint8_t read_back;

    old_mode = radio_read_reg(SX127X_REG_OPMODE);
    test_mode = (uint8_t)((old_mode ^ 0x01U) | SX127X_OPMODE_LONG_RANGE);

    radio_write_reg(SX127X_REG_OPMODE, test_mode);
    read_back = radio_read_reg(SX127X_REG_OPMODE);
    radio_write_reg(SX127X_REG_OPMODE, old_mode);

    return (read_back == test_mode);
}

static void radio_read_fifo(uint8_t *buffer, uint8_t len)
{
    uint8_t i;
    if ((buffer == NULL) || (len == 0U)) {
        return;
    }
    radio_nss(false);
    delay_cycles(40UL);
    (void)spi1_xfer((uint8_t)(SX127X_REG_FIFO & 0x7FU));
    for (i = 0U; i < len; i++) {
        buffer[i] = spi1_xfer(0x00U);
    }
    delay_cycles(40UL);
    radio_nss(true);
}

static void radio_write_fifo(const uint8_t *buffer, uint8_t len)
{
    uint8_t i;
    if ((buffer == NULL) || (len == 0U)) {
        return;
    }
    radio_nss(false);
    delay_cycles(40UL);
    (void)spi1_xfer((uint8_t)(SX127X_REG_FIFO | 0x80U));
    for (i = 0U; i < len; i++) {
        (void)spi1_xfer(buffer[i]);
    }
    delay_cycles(40UL);
    radio_nss(true);
}

static void radio_set_rf_switch(bool tx_on)
{
    (void)tx_on;
#if RADIO_USE_RF_SWITCH
    if (s_profile == NULL) {
        return;
    }
    /*
     * Conservative dual-control RF switch policy:
     * - TX: rf_tx=1, rf_rx=0
     * - RX: rf_tx=0, rf_rx=1
     */
    if ((s_profile->rf_tx_port != NULL) && (s_profile->rf_tx_pin != PIN_NONE)) {
        gpio_write(s_profile->rf_tx_port, s_profile->rf_tx_pin, tx_on);
    }
    if ((s_profile->rf_rx_port != NULL) && (s_profile->rf_rx_pin != PIN_NONE)) {
        gpio_write(s_profile->rf_rx_port, s_profile->rf_rx_pin, !tx_on);
    }
#endif
}

static void radio_init_rf_switch_pins(const radio_profile_t *profile)
{
#if RADIO_USE_RF_SWITCH
    if (profile == NULL) {
        return;
    }

    if ((profile->rf_tx_port != NULL) && (profile->rf_tx_pin != PIN_NONE)) {
        gpio_config_output(profile->rf_tx_port, profile->rf_tx_pin);
        gpio_write(profile->rf_tx_port, profile->rf_tx_pin, false);
    }

    if ((profile->rf_rx_port != NULL) && (profile->rf_rx_pin != PIN_NONE)) {
        gpio_config_output(profile->rf_rx_port, profile->rf_rx_pin);
        gpio_write(profile->rf_rx_port, profile->rf_rx_pin, true);
    }
#else
    (void)profile;
#endif
}

static void radio_reset(void)
{
    if ((s_profile == NULL) || (s_profile->rst_port == NULL) || (s_profile->rst_pin == PIN_NONE)) {
        return;
    }

    /* SX127x reset is released by Hi-Z, not by actively driving high. */
    gpio_config_output(s_profile->rst_port, s_profile->rst_pin);
    gpio_write(s_profile->rst_port, s_profile->rst_pin, false);
    delay_cycles(24000UL);
    gpio_config_input(s_profile->rst_port, s_profile->rst_pin);
    delay_cycles(120000UL);
}

static void radio_apply_profile(const radio_profile_t *profile, uint8_t spi_af)
{
    (void)spi_af;
    s_profile = profile;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN;

    gpio_config_output(profile->sck_port, profile->sck_pin);
    gpio_config_input(profile->miso_port, profile->miso_pin);
    gpio_config_output(profile->mosi_port, profile->mosi_pin);
    gpio_write(profile->sck_port, profile->sck_pin, s_spi_cpol);
    gpio_write(profile->mosi_port, profile->mosi_pin, false);

    gpio_config_output(profile->nss_port, profile->nss_pin);
    gpio_write(profile->nss_port, profile->nss_pin, true);

    if (profile->rst_pin != PIN_NONE) {
        gpio_config_input(profile->rst_port, profile->rst_pin);
    }

    if ((RADIO_USE_TCXO != 0U) && (profile->tcxo_port != NULL) && (profile->tcxo_pin != PIN_NONE)) {
        gpio_config_output(profile->tcxo_port, profile->tcxo_pin);
        gpio_write(profile->tcxo_port, profile->tcxo_pin, true);
        /* Let TCXO settle before probing version register. */
        delay_cycles(120000UL);
    }

    spi1_init();
}

static bool radio_detect(void)
{
    typedef struct {
        uint32_t br;
        bool cpol;
        bool cpha;
    } spi_probe_cfg_t;

    static const spi_probe_cfg_t spi_cfgs[] = {
        {SPI_CR1_BR_DIV64, false, false},
        {SPI_CR1_BR_DIV256, false, false},
        {SPI_CR1_BR_DIV64, true, true},
        {SPI_CR1_BR_DIV256, true, true},
    };

    uint8_t idx;
    uint8_t scan;
    uint8_t spi_idx;
    uint8_t attempt;
    uint8_t version;
    uint8_t best_version;
    uint8_t valid_hits;
    uint8_t profile_count = (uint8_t)(sizeof(s_profiles) / sizeof(s_profiles[0]));
    uint8_t scan_limit = profile_count;

    s_profile = NULL;
    s_profile_index = 0xFFU;
    s_radio_version = 0U;
    s_probe_count = 0U;

    if (profile_count > (uint8_t)sizeof(s_probe_profile)) {
        profile_count = (uint8_t)sizeof(s_probe_profile);
    }

    /* Conservative default: mode 0 at moderate speed. */
    s_spi_default_br = SPI_CR1_BR_DIV64;
    s_spi_default_cpol = false;
    s_spi_default_cpha = false;

    if ((RADIO_FORCE_PROFILE_ONLY != 0U) && (RADIO_FORCE_PROFILE_INDEX >= profile_count)) {
        return false;
    }
    if (RADIO_FORCE_PROFILE_ONLY != 0U) {
        scan_limit = 1U;
    }

    for (scan = 0U; scan < scan_limit; scan++) {
        if (RADIO_FORCE_PROFILE_ONLY != 0U) {
            idx = RADIO_FORCE_PROFILE_INDEX;
        } else {
            idx = (uint8_t)((scan + RADIO_PROFILE_PREFERRED) % profile_count);
        }

        radio_apply_profile(&s_profiles[idx], 0U);
        best_version = 0x00U;
        valid_hits = 0U;

        for (spi_idx = 0U; spi_idx < (uint8_t)(sizeof(spi_cfgs) / sizeof(spi_cfgs[0])); spi_idx++) {
            for (attempt = 0U; attempt < 8U; attempt++) {
                spi1_init_cfg(spi_cfgs[spi_idx].br, spi_cfgs[spi_idx].cpol, spi_cfgs[spi_idx].cpha);
                version = radio_read_reg(SX127X_REG_VERSION);
                if ((version == SX127X_VERSION_SX1272) || (version == SX127X_VERSION_SX1276)) {
                    valid_hits++;
                    best_version = version;
                } else {
                    valid_hits = 0U;
                    if (best_version == 0x00U) {
                        best_version = version;
                    }
                }

                if (valid_hits >= 2U) {
                    if (!radio_rw_check()) {
                        valid_hits = 0U;
                        best_version = version;
                        radio_reset();
                        continue;
                    }
                    best_version = version;
                    break;
                }

                radio_reset();
                delay_cycles(8000UL);
            }

            if (valid_hits >= 2U) {
                break;
            }
        }

        version = best_version;

        s_probe_profile[s_probe_count] = idx;
        s_probe_version[s_probe_count] = version;
        s_probe_af[s_probe_count] = (valid_hits >= 2U) ? (uint8_t)spi_idx : 0xFFU;
        s_probe_count++;

        if (valid_hits >= 2U) {
            s_profile_index = idx;
            s_radio_version = version;
            s_spi_default_br = spi_cfgs[spi_idx].br;
            s_spi_default_cpol = spi_cfgs[spi_idx].cpol;
            s_spi_default_cpha = spi_cfgs[spi_idx].cpha;
#if RADIO_FORCE_SLOW_SPI
            s_spi_default_br = SPI_CR1_BR_DIV256;
#endif
            spi1_init();
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
        /* SX1272 LoRa setup: BW125 / CR4:5 / explicit / CRC on / AGC on. */
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

    radio_write_reg(SX127X_REG_SYMB_TIMEOUT, 0x08U);
    radio_write_reg(SX127X_REG_PREAMBLE_MSB, 0x00U);
    radio_write_reg(SX127X_REG_PREAMBLE_LSB, 0x08U);
    radio_write_reg(SX127X_REG_PAYLOAD_LEN, 0xFFU);
    radio_write_reg(SX127X_REG_MAX_PAYLOAD, 0xFFU);
    radio_write_reg(SX127X_REG_HOP_PERIOD, 0x00U);
    radio_write_reg(SX127X_REG_SYNC_WORD, LORA_SYNC_WORD_PRIVATE);
    radio_write_reg(SX127X_REG_DIO_MAPPING1, 0x00U);
    radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
}

static void radio_start_rx_continuous(void)
{
    radio_set_rf_switch(false);
    /*
     * Move through STDBY before re-entering RX to avoid sticky RX state
     * observed after first packet on some SX1272 boards.
     */
    radio_write_reg(SX127X_REG_OPMODE, SX127X_OPMODE_LONG_RANGE | SX127X_OPMODE_STDBY);
    radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
    radio_write_reg(SX127X_REG_FIFO_ADDR_PTR, radio_read_reg(SX127X_REG_FIFO_RX_BASE));
    radio_write_reg(SX127X_REG_DIO_MAPPING1, 0x00U);
    radio_write_reg(SX127X_REG_OPMODE, SX127X_OPMODE_LONG_RANGE | SX127X_OPMODE_RX_CONT);
    s_radio_state = RADIO_STATE_RX;
    s_rx_guard_ms = s_now_ms;
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
    s_last_rx_event_ms = s_now_ms;
    s_last_recover_ms = s_now_ms;

    debug_port_enable_swd_only();
    if (!radio_detect()) {
        return false;
    }

    radio_init_rf_switch_pins(s_profile);
    radio_configure_lora();
    radio_start_rx_continuous();
    return true;
}

void platform_radio_process(void)
{
    uint8_t irq_flags;
    uint8_t opmode;

    if ((s_profile == NULL) || (s_radio_state == RADIO_STATE_OFF)) {
        return;
    }

    irq_flags = radio_read_reg(SX127X_REG_IRQ_FLAGS);

    if (s_radio_state == RADIO_STATE_TX) {
        if ((irq_flags & SX127X_IRQ_TX_DONE) != 0U) {
            radio_write_reg(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_TX_DONE);
            radio_start_rx_continuous();
            irq_flags &= (uint8_t)~SX127X_IRQ_TX_DONE;
        } else if ((s_now_ms - s_tx_start_ms) > LORA_TX_RECOVER_MS) {
            /*
             * Fast self-recovery: if TX_DONE is never observed, force radio back to RX.
             * Airtime at SF7/125k for these payloads is far below this guard window.
             */
            radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
            radio_write_reg(SX127X_REG_OPMODE, SX127X_OPMODE_LONG_RANGE | SX127X_OPMODE_STDBY);
            radio_start_rx_continuous();
            return;
        } else if ((s_now_ms - s_tx_start_ms) > LORA_TX_TIMEOUT_MS) {
            radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
            radio_start_rx_continuous();
            return;
        }
    }

    if ((irq_flags & SX127X_IRQ_RX_DONE) != 0U) {
        uint8_t len;
        uint8_t fifo_addr;
        int8_t snr_db;
        int16_t rssi_dbm;

        if ((irq_flags & SX127X_IRQ_PAYLOAD_CRC_ERR) != 0U) {
            radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
            radio_start_rx_continuous();
            return;
        }

        len = radio_read_reg(SX127X_REG_RX_NB_BYTES);
        if (len == 0U) {
            radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
            radio_start_rx_continuous();
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
        s_last_rx_event_ms = s_now_ms;

        /*
         * Explicitly re-arm RX right after FIFO read to avoid single-packet lockups
         * seen on this board/profile combination.
         */
        radio_write_reg(SX127X_REG_IRQ_FLAGS, 0xFFU);
        radio_start_rx_continuous();

        if (s_rx_cb != NULL) {
            s_rx_cb(LINK_PREDECESSOR, s_rx_buffer, len, rssi_dbm, snr_db);
        }

        return;
    }

    if ((irq_flags & SX127X_IRQ_RX_TIMEOUT) != 0U) {
        radio_write_reg(SX127X_REG_IRQ_FLAGS, SX127X_IRQ_RX_TIMEOUT);
        radio_start_rx_continuous();
        return;
    }

    if (s_radio_state == RADIO_STATE_RX) {
        if ((s_now_ms - s_rx_guard_ms) >= LORA_RX_GUARD_MS) {
            opmode = radio_read_reg(SX127X_REG_OPMODE);
            if ((opmode & SX127X_OPMODE_LONG_RANGE) == 0U ||
                (opmode & 0x07U) != SX127X_OPMODE_RX_CONT ||
                (s_now_ms - s_last_rx_event_ms) >= LORA_RX_SOFT_RECOVER_MS) {
                radio_start_rx_continuous();
            }
            s_rx_guard_ms = s_now_ms;
        }

        if (((s_now_ms - s_last_rx_event_ms) >= LORA_RX_STUCK_RESET_MS) &&
            ((s_now_ms - s_last_recover_ms) >= LORA_RX_RECOVER_GAP_MS)) {
            /* Hard recovery from silent RX lock-up observed on some board/profile combos. */
            radio_reset();
            radio_configure_lora();
            radio_start_rx_continuous();
            s_last_recover_ms = s_now_ms;
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

uint8_t platform_radio_probe_count(void)
{
    return s_probe_count;
}

uint8_t platform_radio_probe_profile(uint8_t idx)
{
    if (idx >= s_probe_count) {
        return 0xFFU;
    }
    return s_probe_profile[idx];
}

uint8_t platform_radio_probe_version(uint8_t idx)
{
    if (idx >= s_probe_count) {
        return 0x00U;
    }
    return s_probe_version[idx];
}

uint8_t platform_radio_probe_af(uint8_t idx)
{
    if (idx >= s_probe_count) {
        return 0xFFU;
    }
    return s_probe_af[idx];
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
