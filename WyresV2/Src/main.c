#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "platform_port.h"

/*
 * WyresV2 W_BASE V2 REVC bring-up target: STM32L151CC (Cortex-M3)
 * - LED1: PA0
 * - LED2: PB3 (may be unavailable depending on radio pin profile)
 * - UART debug: USART1 TX=PA9 RX=PA10
 */

#define PERIPH_BASE         0x40000000UL
#define AHBPERIPH_BASE      (PERIPH_BASE + 0x00020000UL)
#define APB2PERIPH_BASE     (PERIPH_BASE + 0x00010000UL)

#define GPIOA_BASE          (AHBPERIPH_BASE + 0x0000UL)
#define GPIOB_BASE          (AHBPERIPH_BASE + 0x0400UL)
#define RCC_BASE            (AHBPERIPH_BASE + 0x3800UL)
#define USART1_BASE         (APB2PERIPH_BASE + 0x3800UL)

#define LED1_PIN            (1UL << 0)   /* PA0  */
#define LED2_PIN            (1UL << 3)   /* PB3  */

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
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t GTPR;
} USART_TypeDef;

#define GPIOA               ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB               ((GPIO_TypeDef *)GPIOB_BASE)
#define RCC                 ((RCC_TypeDef *)RCC_BASE)
#define USART1              ((USART_TypeDef *)USART1_BASE)

#define RCC_AHBENR_GPIOAEN  (1UL << 0)
#define RCC_AHBENR_GPIOBEN  (1UL << 1)
#define RCC_APB2ENR_USART1EN (1UL << 14)
#define RCC_CR_HSION        (1UL << 0)
#define RCC_CR_HSIRDY       (1UL << 1)
#define RCC_CFGR_SW_MASK    (3UL << 0)
#define RCC_CFGR_SW_HSI     (1UL << 0)
#define RCC_CFGR_SWS_MASK   (3UL << 2)
#define RCC_CFGR_SWS_HSI    (1UL << 2)

#define USART_CR1_UE        (1UL << 13)
#define USART_CR1_TE        (1UL << 3)
#define USART_CR1_RE        (1UL << 2)
#define USART_SR_TXE        (1UL << 7)
#define USART_SR_TC         (1UL << 6)

#define APP_ENABLE_PERIODIC_WYRES_PING  0U
#define APP_PING_PERIOD_MS              3000U
#define APP_TX_GUARD_AFTER_RX_MS        1500U
#define APP_PONG_DELAY_MS               120U
#define APP_ENABLE_PONG_REPLY           0U

static volatile uint32_t s_rx_count = 0U;
static volatile uint32_t s_tx_count = 0U;
static volatile uint32_t s_last_rx_ms = 0U;
static volatile uint8_t s_pong_pending = 0U;
static volatile uint32_t s_pong_due_ms = 0U;
static const uint8_t s_pong_msg[] = "WYRES_PONG";

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles-- != 0UL) {
        __asm volatile ("nop");
    }
}

static void delay_ms(uint32_t ms)
{
    while (ms-- != 0UL) {
        delay_cycles(12000UL);
    }
}

static void gpio_set_output(GPIO_TypeDef *gpio, uint32_t pin_index)
{
    const uint32_t shift = pin_index * 2UL;
    gpio->MODER &= ~(3UL << shift);
    gpio->MODER |= (1UL << shift);
    gpio->OTYPER &= ~(1UL << pin_index);
    gpio->OSPEEDR &= ~(3UL << shift);
    gpio->PUPDR &= ~(3UL << shift);
}

static void led_write(uint8_t on)
{
    if (on != 0U) {
        GPIOA->BSRR = LED1_PIN;
        GPIOB->BSRR = LED2_PIN;
    } else {
        GPIOA->BSRR = (LED1_PIN << 16);
        GPIOB->BSRR = (LED2_PIN << 16);
    }
}

static void led_toggle(void)
{
    GPIOA->ODR ^= LED1_PIN;
}

static void system_clock_init_hsi_16mhz(void)
{
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0UL) {
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_MASK) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_HSI) {
    }
}

static void uart1_init_115200(void)
{
    uint32_t temp;

    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9/PA10 -> AF7 */
    temp = GPIOA->MODER;
    temp &= ~((3UL << (9UL * 2UL)) | (3UL << (10UL * 2UL)));
    temp |=  ((2UL << (9UL * 2UL)) | (2UL << (10UL * 2UL)));
    GPIOA->MODER = temp;

    temp = GPIOA->AFR[1];
    temp &= ~((0xFUL << ((9UL - 8UL) * 4UL)) | (0xFUL << ((10UL - 8UL) * 4UL)));
    temp |=  ((7UL << ((9UL - 8UL) * 4UL)) | (7UL << ((10UL - 8UL) * 4UL)));
    GPIOA->AFR[1] = temp;

    GPIOA->OSPEEDR |= (3UL << (9UL * 2UL)) | (3UL << (10UL * 2UL));
    GPIOA->PUPDR &= ~((3UL << (9UL * 2UL)) | (3UL << (10UL * 2UL)));
    GPIOA->PUPDR |=  ((1UL << (9UL * 2UL)) | (1UL << (10UL * 2UL)));

    USART1->CR1 = 0UL;
    USART1->CR2 = 0UL;
    USART1->CR3 = 0UL;
    USART1->BRR = (16000000UL + (115200UL / 2UL)) / 115200UL;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void uart1_write_char(char c)
{
    while ((USART1->SR & USART_SR_TXE) == 0UL) {
    }
    USART1->DR = (uint32_t)(uint8_t)c;
}

static void uart1_write_str(const char *s)
{
    if (s == NULL) {
        return;
    }

    while (*s != '\0') {
        if (*s == '\n') {
            uart1_write_char('\r');
        }
        uart1_write_char(*s++);
    }

    while ((USART1->SR & USART_SR_TC) == 0UL) {
    }
}

static void uart1_write_u32(uint32_t value)
{
    char buf[11];
    uint8_t i = 0U;

    if (value == 0U) {
        uart1_write_char('0');
        return;
    }

    while ((value > 0U) && (i < (uint8_t)sizeof(buf))) {
        buf[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (i > 0U) {
        uart1_write_char(buf[--i]);
    }
}

static void uart1_write_i32(int32_t value)
{
    if (value < 0) {
        uart1_write_char('-');
        uart1_write_u32((uint32_t)(-value));
    } else {
        uart1_write_u32((uint32_t)value);
    }
}

static void uart1_write_printable(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t c;

    if (data == NULL) {
        return;
    }

    for (i = 0U; i < len; i++) {
        c = data[i];
        if ((c < 32U) || (c > 126U)) {
            c = '.';
        }
        uart1_write_char((char)c);
    }
}

static bool payload_starts_with(const uint8_t *data, uint16_t len, const char *prefix)
{
    uint16_t prefix_len;

    if ((data == NULL) || (prefix == NULL)) {
        return false;
    }

    prefix_len = (uint16_t)strlen(prefix);
    if (len < prefix_len) {
        return false;
    }

    return (memcmp(data, prefix, prefix_len) == 0);
}

static void app_radio_rx_cb(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr)
{
    (void)from;

    s_rx_count++;
    s_last_rx_ms = platform_millis();

    uart1_write_str("RX #");
    uart1_write_u32(s_rx_count);
    uart1_write_str(" RSSI=");
    uart1_write_i32((int32_t)rssi);
    uart1_write_str(" SNR=");
    uart1_write_i32((int32_t)snr);
    uart1_write_str(" LEN=");
    uart1_write_u32((uint32_t)len);
    uart1_write_str(" DATA=\"");
    uart1_write_printable(data, len);
    uart1_write_str("\"\r\n");

    if ((APP_ENABLE_PONG_REPLY != 0U) && payload_starts_with(data, len, "LORA_PING")) {
        s_pong_pending = 1U;
        s_pong_due_ms = platform_millis() + APP_PONG_DELAY_MS;
        uart1_write_str("TXQ: WYRES_PONG\r\n");
    }
}

void platform_port_set_time_ms(uint32_t now_ms);

void Error_Handler(void)
{
    while (1) {
        led_toggle();
        delay_ms(150U);
    }
}

int main(void)
{
    uint32_t i;
    uint32_t now_ms = 0U;
    uint32_t last_status_ms = 0U;
    uint32_t last_ping_ms = 0U;
    bool radio_ok;
    bool send_ok;
    static const uint8_t ping_msg[] = "WYRES_PING";

    system_clock_init_hsi_16mhz();

    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
    gpio_set_output(GPIOA, 0U);
    led_write(0U);

    for (i = 0U; i < 3U; i++) {
        led_write(1U);
        delay_ms(120U);
        led_write(0U);
        delay_ms(120U);
    }

    uart1_init_115200();
    uart1_write_str("BOOT WYRESV2 STM32L151\r\n");
    uart1_write_str("BUILD: RX_STREAM_REARM_SOFT\r\n");
    uart1_write_str("MODE: LORA INTEROP TEST\r\n");

    radio_ok = platform_radio_init(app_radio_rx_cb);
    if (radio_ok) {
        uart1_write_str("RADIO OK version=");
        uart1_write_u32((uint32_t)platform_radio_version());
        uart1_write_str(" profile=");
        uart1_write_u32((uint32_t)platform_radio_profile());
        uart1_write_str(" spiCfg=");
        uart1_write_u32((uint32_t)platform_radio_probe_af(0U));
        if ((platform_radio_version() != 0x12U) && (platform_radio_version() != 0x22U)) {
            uart1_write_str(" (fallback)");
        }
        uart1_write_str("\r\n");
    } else {
        uint8_t probe_i;
        uint8_t probe_n = platform_radio_probe_count();
        uart1_write_str("RADIO INIT FAIL (check SPI/NSS/RESET + TCXO power pin)\r\n");
        uart1_write_str("Probe results: ");
        uart1_write_u32((uint32_t)probe_n);
        uart1_write_str(" profiles\r\n");
        for (probe_i = 0U; probe_i < probe_n; probe_i++) {
            uart1_write_str("  p=");
            uart1_write_u32((uint32_t)platform_radio_probe_profile(probe_i));
            uart1_write_str(" af=");
            uart1_write_u32((uint32_t)platform_radio_probe_af(probe_i));
            uart1_write_str(" ver=0x");
            uart1_write_char("0123456789ABCDEF"[(platform_radio_probe_version(probe_i) >> 4) & 0x0FU]);
            uart1_write_char("0123456789ABCDEF"[platform_radio_probe_version(probe_i) & 0x0FU]);
            uart1_write_str("\r\n");
        }
    }

    while (1) {
        platform_port_set_time_ms(now_ms);
        platform_radio_process();

        if ((radio_ok) && (s_pong_pending != 0U) && ((int32_t)(now_ms - s_pong_due_ms) >= 0)) {
            send_ok = platform_radio_send(LINK_SUCCESSOR, s_pong_msg, (uint16_t)(sizeof(s_pong_msg) - 1U));
            if (send_ok) {
                s_pong_pending = 0U;
                s_tx_count++;
                uart1_write_str("TX: WYRES_PONG\r\n");
            } else {
                s_pong_due_ms = now_ms + 80U;
            }
        }

        if ((radio_ok) &&
            (APP_ENABLE_PERIODIC_WYRES_PING != 0U) &&
            ((now_ms - last_ping_ms) >= APP_PING_PERIOD_MS) &&
            ((now_ms - s_last_rx_ms) >= APP_TX_GUARD_AFTER_RX_MS)) {
            send_ok = platform_radio_send(LINK_SUCCESSOR, ping_msg, (uint16_t)(sizeof(ping_msg) - 1U));
            if (send_ok) {
                s_tx_count++;
                uart1_write_str("TX: WYRES_PING\r\n");
            } else {
                uart1_write_str("TX: WYRES_PING FAIL\r\n");
            }
            last_ping_ms = now_ms;
        }

        if ((now_ms - last_status_ms) >= 1000U) {
            uint8_t irq = platform_radio_dbg_last_irq_flags();
            uint8_t opm = platform_radio_dbg_last_opmode();
            uart1_write_str("ALIVE t=");
            uart1_write_u32(now_ms);
            uart1_write_str("ms TX=");
            uart1_write_u32(s_tx_count);
            uart1_write_str(" RX=");
            uart1_write_u32(s_rx_count);
            uart1_write_str(" RD=");
            uart1_write_u32(platform_radio_dbg_rx_done_count());
            uart1_write_str(" CE=");
            uart1_write_u32(platform_radio_dbg_crc_err_count());
            uart1_write_str(" TO=");
            uart1_write_u32(platform_radio_dbg_rx_timeout_count());
            uart1_write_str(" RA=");
            uart1_write_u32(platform_radio_dbg_rearm_count());
            uart1_write_str(" HR=");
            uart1_write_u32(platform_radio_dbg_hard_reset_count());
            uart1_write_str(" IRQ=0x");
            uart1_write_char("0123456789ABCDEF"[(irq >> 4) & 0x0FU]);
            uart1_write_char("0123456789ABCDEF"[irq & 0x0FU]);
            uart1_write_str(" OPM=0x");
            uart1_write_char("0123456789ABCDEF"[(opm >> 4) & 0x0FU]);
            uart1_write_char("0123456789ABCDEF"[opm & 0x0FU]);
            uart1_write_str(" BAT=");
            uart1_write_u32(platform_battery_mv());
            uart1_write_str("mV\r\n");
            last_status_ms = now_ms;
        }

        now_ms += 100U;
        delay_ms(100U);
    }
}
