#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "platform_port.h"
#include "wyresv2_node.h"

/*
 * WyresV2 W_BASE V2 REVC bring-up target: STM32L151CC (Cortex-M3)
 * - LED1: PA0
 * - LED2: PB3 (may be unavailable depending on radio pin profile)
 * - UART debug: USART1 TX=PA9 RX=PA10
 */

#define PERIPH_BASE          0x40000000UL
#define AHBPERIPH_BASE       (PERIPH_BASE + 0x00020000UL)
#define APB2PERIPH_BASE      (PERIPH_BASE + 0x00010000UL)

#define GPIOA_BASE           (AHBPERIPH_BASE + 0x0000UL)
#define GPIOB_BASE           (AHBPERIPH_BASE + 0x0400UL)
#define RCC_BASE             (AHBPERIPH_BASE + 0x3800UL)
#define USART1_BASE          (APB2PERIPH_BASE + 0x3800UL)

#define LED1_PIN             (1UL << 0)   /* PA0 */
#define LED2_PIN             (1UL << 3)   /* PB3 */

#define STM32L1_UID_BASE     0x1FF80050UL

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

#define GPIOA                ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB                ((GPIO_TypeDef *)GPIOB_BASE)
#define RCC                  ((RCC_TypeDef *)RCC_BASE)
#define USART1               ((USART_TypeDef *)USART1_BASE)

#define RCC_AHBENR_GPIOAEN   (1UL << 0)
#define RCC_AHBENR_GPIOBEN   (1UL << 1)
#define RCC_APB2ENR_USART1EN (1UL << 14)
#define RCC_CR_HSION         (1UL << 0)
#define RCC_CR_HSIRDY        (1UL << 1)
#define RCC_CFGR_SW_MASK     (3UL << 0)
#define RCC_CFGR_SW_HSI      (1UL << 0)
#define RCC_CFGR_SWS_MASK    (3UL << 2)
#define RCC_CFGR_SWS_HSI     (1UL << 2)

#define USART_CR1_UE         (1UL << 13)
#define USART_CR1_TE         (1UL << 3)
#define USART_CR1_RE         (1UL << 2)
#define USART_CR1_RXNEIE     (1UL << 5)
#define USART_SR_ORE         (1UL << 3)
#define USART_SR_RXNE        (1UL << 5)
#define USART_SR_TXE         (1UL << 7)
#define USART_SR_TC          (1UL << 6)

#define NVIC_ISER_BASE       0xE000E100UL
#define NVIC_ISER            ((volatile uint32_t *)NVIC_ISER_BASE)
#define USART1_IRQ_NUMBER    37U
#define UART_RX_RING_SIZE    128U

#define SYSTICK_BASE         0xE000E010UL
#define SYSTICK_CSR          (*(volatile uint32_t *)(SYSTICK_BASE + 0x0UL))
#define SYSTICK_RVR          (*(volatile uint32_t *)(SYSTICK_BASE + 0x4UL))
#define SYSTICK_CVR          (*(volatile uint32_t *)(SYSTICK_BASE + 0x8UL))

#define SYSTICK_CSR_ENABLE   (1UL << 0)
#define SYSTICK_CSR_TICKINT  (1UL << 1)
#define SYSTICK_CSR_CLKSRC   (1UL << 2)

/*
 * Network behavior knobs:
 * - Set APP_COORDINATOR_MODE=1 on exactly one board to assign sequential IDs.
 * - Set APP_FIXED_NODE_ID to force a static node ID (disables join client).
 */
#define APP_PROTOCOL_VERSION      1U
#define APP_NODE_ID_UNASSIGNED    0U
#define APP_BROADCAST_ID          0xFFFFU
#define APP_COORDINATOR_ID        1U
#define APP_COORDINATOR_MODE      0U
#define APP_FIXED_NODE_ID         0U

#define APP_ACK_TIMEOUT_MS        700U
#define APP_ACK_MAX_RETRIES       3U
#define APP_ACK_BURST_REPEAT      2U
#define APP_ACK_BURST_DELAY_MS    80U
#define APP_ACK_BURST_GAP_MS      80U
#define APP_RETRY_BACKOFF_MS      120U
#define APP_JOIN_RETRY_MS         2000U
#define APP_HEARTBEAT_PERIOD_MS   3000U
#define APP_NODE_OFFLINE_MS       90000U
#define APP_LOOP_STEP_MS          20U
#define APP_STATUS_PERIOD_MS      1000U

#define APP_UART_LINE_MAX         96U
#define APP_UART_ECHO             0U
#define APP_UART_AUTOSUBMIT_MS    3000U
#define APP_RX_TRACK_SIZE         16U
#define APP_RELAY_TRACK_SIZE      24U
#define APP_JOIN_TABLE_SIZE       16U
#define APP_JOIN_FIRST_ASSIGN_ID  2U
#define APP_JOIN_LAST_ASSIGN_ID   250U

typedef struct {
    bool active;
    uint16_t dst_id;
    uint16_t seq;
    uint8_t retries_left;
    uint8_t raw_len;
    uint32_t deadline_ms;
    uint8_t raw[11U + WYRESV2_MAX_PAYLOAD];
} app_pending_tx_t;

typedef struct {
    bool used;
    uint16_t src_id;
    uint16_t last_seq;
} app_rx_track_entry_t;

typedef struct {
    bool used;
    uint16_t src_id;
    uint16_t seq;
} app_relay_track_entry_t;

typedef struct {
    bool used;
    uint16_t nonce;
    uint16_t node_id;
    uint32_t last_seen_ms;
    bool online;
} app_join_entry_t;

typedef struct {
    bool active;
    uint16_t dst_id;
    uint16_t seq;
    uint8_t repeats_left;
    uint32_t next_tx_ms;
} app_ack_burst_t;

static volatile uint32_t s_rx_count = 0U;
static volatile uint32_t s_tx_count = 0U;
static volatile uint32_t s_retry_count = 0U;
static volatile uint32_t s_ack_rx_count = 0U;
static volatile uint32_t s_ack_tx_count = 0U;
static volatile uint32_t s_ack_timeout_count = 0U;
static volatile uint32_t s_text_rx_count = 0U;
static volatile uint32_t s_join_req_count = 0U;
static volatile uint32_t s_join_ack_count = 0U;

static uint16_t s_node_id = APP_NODE_ID_UNASSIGNED;
static uint16_t s_default_dst_id = APP_COORDINATOR_ID;
static uint16_t s_next_seq = 1U;
static uint16_t s_join_nonce = 0U;
static uint16_t s_next_assigned_id = APP_JOIN_FIRST_ASSIGN_ID;
static uint32_t s_last_join_req_ms = 0U;
static uint32_t s_last_heartbeat_ms = 0U;
static bool s_joined = false;

static app_pending_tx_t s_pending_tx;
static app_ack_burst_t s_pending_ack_burst;
static app_rx_track_entry_t s_rx_track[APP_RX_TRACK_SIZE];
static app_relay_track_entry_t s_relay_track[APP_RELAY_TRACK_SIZE];
static app_join_entry_t s_join_table[APP_JOIN_TABLE_SIZE];
static uint8_t s_rx_track_insert_idx = 0U;
static uint8_t s_relay_track_insert_idx = 0U;
static char s_uart_line[APP_UART_LINE_MAX];
static uint8_t s_uart_line_len = 0U;
static uint32_t s_uart_last_rx_ms = 0U;
static bool s_join_req_next_direct = false;
static volatile uint32_t s_system_ms = 0U;
static volatile uint8_t s_uart_rx_ring[UART_RX_RING_SIZE];
static volatile uint8_t s_uart_rx_head = 0U;
static volatile uint8_t s_uart_rx_tail = 0U;

static void app_print_status(uint32_t now_ms);

static void nvic_enable_irq(uint8_t irqn)
{
    NVIC_ISER[irqn / 32U] = (uint32_t)1UL << (irqn % 32U);
}

static void systick_init_1khz(void)
{
    /* 16 MHz core clock -> 1 ms tick */
    SYSTICK_RVR = 16000UL - 1UL;
    SYSTICK_CVR = 0UL;
    SYSTICK_CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSRC;
}

void SysTick_Handler(void)
{
    s_system_ms++;
}

void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;

    while ((sr & USART_SR_RXNE) != 0UL) {
        uint8_t c = (uint8_t)(USART1->DR & 0xFFU);
        uint8_t next = (uint8_t)(s_uart_rx_head + 1U);
        if (next >= UART_RX_RING_SIZE) {
            next = 0U;
        }
        if (next != s_uart_rx_tail) {
            s_uart_rx_ring[s_uart_rx_head] = c;
            s_uart_rx_head = next;
        }
        sr = USART1->SR;
    }

    if ((sr & USART_SR_ORE) != 0UL) {
        (void)USART1->DR;
    }
}

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
    s_uart_rx_head = 0U;
    s_uart_rx_tail = 0U;
    while ((USART1->SR & USART_SR_RXNE) != 0UL) {
        (void)USART1->DR;
    }

    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    nvic_enable_irq(USART1_IRQ_NUMBER);
}

static void uart1_write_char(char c)
{
    while ((USART1->SR & USART_SR_TXE) == 0UL) {
    }
    USART1->DR = (uint32_t)(uint8_t)c;
}

static bool uart1_try_read_char(char *out_char)
{
    uint8_t tail;

    if (out_char == NULL) {
        return false;
    }

    tail = s_uart_rx_tail;
    if (tail != s_uart_rx_head) {
        *out_char = (char)s_uart_rx_ring[tail];
        tail++;
        if (tail >= UART_RX_RING_SIZE) {
            tail = 0U;
        }
        s_uart_rx_tail = tail;
        return true;
    }

    if ((USART1->SR & USART_SR_RXNE) != 0UL) {
        *out_char = (char)(USART1->DR & 0xFFU);
        return true;
    }

    return false;
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

static void uart1_write_hex8(uint8_t value)
{
    uart1_write_char("0123456789ABCDEF"[(value >> 4) & 0x0FU]);
    uart1_write_char("0123456789ABCDEF"[value & 0x0FU]);
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

static bool is_space_char(char c)
{
    return (c == ' ') || (c == '\t');
}

static char *skip_spaces(char *p)
{
    if (p == NULL) {
        return NULL;
    }
    while (is_space_char(*p)) {
        p++;
    }
    return p;
}

static void rstrip_spaces(char *p)
{
    size_t n;

    if (p == NULL) {
        return;
    }

    n = strlen(p);
    while (n > 0U) {
        char c = p[n - 1U];
        if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n')) {
            p[n - 1U] = '\0';
            n--;
        } else {
            break;
        }
    }
}

static bool parse_u16_token(char **cursor, uint16_t *out_value)
{
    char *p;
    uint32_t value = 0U;
    bool has_digit = false;

    if ((cursor == NULL) || (out_value == NULL) || (*cursor == NULL)) {
        return false;
    }

    p = skip_spaces(*cursor);
    while ((*p >= '0') && (*p <= '9')) {
        has_digit = true;
        value = (value * 10U) + (uint32_t)(*p - '0');
        if (value > 65535U) {
            return false;
        }
        p++;
    }

    if (!has_digit) {
        return false;
    }

    *out_value = (uint16_t)value;
    *cursor = p;
    return true;
}

static uint16_t app_read_uid_nonce(void)
{
    const volatile uint32_t *uid = (const volatile uint32_t *)STM32L1_UID_BASE;
    uint32_t mix = uid[0] ^ uid[1] ^ uid[2];
    mix ^= (mix >> 16);
    if ((mix & 0xFFFFU) == 0U) {
        mix ^= 0x5A5A5A5AU;
    }
    return (uint16_t)(mix & 0xFFFFU);
}

static uint8_t app_serialize_frame(const wyresv2_frame_t *frame, uint8_t *out, uint8_t out_max)
{
    uint8_t need;
    uint8_t i;

    if ((frame == NULL) || (out == NULL)) {
        return 0U;
    }

    if (frame->payload_len > WYRESV2_MAX_PAYLOAD) {
        return 0U;
    }

    need = (uint8_t)(11U + frame->payload_len);
    if (need > out_max) {
        return 0U;
    }

    out[0] = frame->version;
    out[1] = frame->type;
    out[2] = (uint8_t)(frame->src_id & 0xFFU);
    out[3] = (uint8_t)((frame->src_id >> 8) & 0xFFU);
    out[4] = (uint8_t)(frame->dst_id & 0xFFU);
    out[5] = (uint8_t)((frame->dst_id >> 8) & 0xFFU);
    out[6] = (uint8_t)(frame->seq & 0xFFU);
    out[7] = (uint8_t)((frame->seq >> 8) & 0xFFU);
    out[8] = frame->ttl;
    out[9] = frame->flags;
    out[10] = frame->payload_len;

    for (i = 0U; i < frame->payload_len; i++) {
        out[11U + i] = frame->payload[i];
    }

    return need;
}

static bool app_parse_frame(wyresv2_frame_t *frame, const uint8_t *raw, uint16_t raw_len)
{
    uint8_t i;
    uint8_t payload_len;
    uint16_t expected;

    if ((frame == NULL) || (raw == NULL) || (raw_len < 11U)) {
        return false;
    }

    payload_len = raw[10];
    if (payload_len > WYRESV2_MAX_PAYLOAD) {
        return false;
    }

    expected = (uint16_t)11U + (uint16_t)payload_len;
    if (raw_len != expected) {
        return false;
    }

    frame->version = raw[0];
    frame->type = raw[1];
    frame->src_id = (uint16_t)raw[2] | ((uint16_t)raw[3] << 8);
    frame->dst_id = (uint16_t)raw[4] | ((uint16_t)raw[5] << 8);
    frame->seq = (uint16_t)raw[6] | ((uint16_t)raw[7] << 8);
    frame->ttl = raw[8];
    frame->flags = raw[9];
    frame->payload_len = payload_len;

    for (i = 0U; i < payload_len; i++) {
        frame->payload[i] = raw[11U + i];
    }

    return true;
}

static bool app_send_raw_dir(link_direction_t dir, const uint8_t *raw, uint8_t raw_len)
{
    if ((raw == NULL) || (raw_len == 0U)) {
        return false;
    }

    if (!platform_radio_send(dir, raw, raw_len)) {
        return false;
    }

    s_tx_count++;
    return true;
}

static bool app_send_raw(const uint8_t *raw, uint8_t raw_len)
{
    return app_send_raw_dir(LINK_SUCCESSOR, raw, raw_len);
}

static bool app_send_frame(const wyresv2_frame_t *frame)
{
    uint8_t raw[11U + WYRESV2_MAX_PAYLOAD];
    uint8_t raw_len;

    raw_len = app_serialize_frame(frame, raw, (uint8_t)sizeof(raw));
    if (raw_len == 0U) {
        return false;
    }

    return app_send_raw(raw, raw_len);
}

static void app_frame_init(wyresv2_frame_t *frame, uint8_t type, uint16_t src_id, uint16_t dst_id, uint16_t seq)
{
    if (frame == NULL) {
        return;
    }

    memset(frame, 0, sizeof(*frame));
    frame->version = APP_PROTOCOL_VERSION;
    frame->type = type;
    frame->src_id = src_id;
    frame->dst_id = dst_id;
    frame->seq = seq;
    frame->ttl = 8U;
    frame->flags = 0U;
}

static bool app_is_duplicate(uint16_t src_id, uint16_t seq)
{
    uint8_t i;
    app_rx_track_entry_t *slot;

    for (i = 0U; i < APP_RX_TRACK_SIZE; i++) {
        slot = &s_rx_track[i];
        if (slot->used && (slot->src_id == src_id)) {
            if (slot->last_seq == seq) {
                return true;
            }
            slot->last_seq = seq;
            return false;
        }
    }

    slot = &s_rx_track[s_rx_track_insert_idx];
    slot->used = true;
    slot->src_id = src_id;
    slot->last_seq = seq;
    s_rx_track_insert_idx++;
    if (s_rx_track_insert_idx >= APP_RX_TRACK_SIZE) {
        s_rx_track_insert_idx = 0U;
    }
    return false;
}

static bool app_relay_is_duplicate(uint16_t src_id, uint16_t seq)
{
    uint8_t i;

    for (i = 0U; i < APP_RELAY_TRACK_SIZE; i++) {
        if (s_relay_track[i].used &&
            (s_relay_track[i].src_id == src_id) &&
            (s_relay_track[i].seq == seq)) {
            return true;
        }
    }

    return false;
}

static void app_relay_remember(uint16_t src_id, uint16_t seq)
{
    app_relay_track_entry_t *slot = &s_relay_track[s_relay_track_insert_idx];

    slot->used = true;
    slot->src_id = src_id;
    slot->seq = seq;
    s_relay_track_insert_idx++;
    if (s_relay_track_insert_idx >= APP_RELAY_TRACK_SIZE) {
        s_relay_track_insert_idx = 0U;
    }
}

static bool app_join_id_in_use(uint16_t node_id)
{
    uint8_t i;

    if ((node_id == APP_NODE_ID_UNASSIGNED) || (node_id == APP_BROADCAST_ID)) {
        return true;
    }

    if ((APP_COORDINATOR_MODE != 0U) && (node_id == APP_COORDINATOR_ID)) {
        return true;
    }

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++) {
        if (s_join_table[i].used && (s_join_table[i].node_id == node_id)) {
            return true;
        }
    }

    return false;
}

static uint16_t app_join_allocate_id(void)
{
    uint16_t candidate = s_next_assigned_id;
    uint16_t tries = (uint16_t)(APP_JOIN_LAST_ASSIGN_ID - APP_JOIN_FIRST_ASSIGN_ID + 1U);

    while (tries-- != 0U) {
        if ((candidate < APP_JOIN_FIRST_ASSIGN_ID) || (candidate > APP_JOIN_LAST_ASSIGN_ID)) {
            candidate = APP_JOIN_FIRST_ASSIGN_ID;
        }

        if (!app_join_id_in_use(candidate)) {
            s_next_assigned_id = (uint16_t)(candidate + 1U);
            return candidate;
        }

        candidate++;
    }

    return APP_NODE_ID_UNASSIGNED;
}

static uint16_t app_join_find_or_assign(uint16_t nonce)
{
    uint8_t i;
    int8_t free_idx = -1;
    uint16_t assigned_id;

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++) {
        if (s_join_table[i].used) {
            if (s_join_table[i].nonce == nonce) {
                return s_join_table[i].node_id;
            }
        } else if (free_idx < 0) {
            free_idx = (int8_t)i;
        }
    }

    if (free_idx < 0) {
        return APP_NODE_ID_UNASSIGNED;
    }

    assigned_id = app_join_allocate_id();
    if (assigned_id == APP_NODE_ID_UNASSIGNED) {
        return APP_NODE_ID_UNASSIGNED;
    }

    s_join_table[(uint8_t)free_idx].used = true;
    s_join_table[(uint8_t)free_idx].nonce = nonce;
    s_join_table[(uint8_t)free_idx].node_id = assigned_id;
    s_join_table[(uint8_t)free_idx].last_seen_ms = platform_millis();
    s_join_table[(uint8_t)free_idx].online = true;
    return assigned_id;
}

static void app_mark_node_seen(uint16_t node_id, uint32_t now_ms)
{
    uint8_t i;

    if (APP_COORDINATOR_MODE == 0U) {
        return;
    }

    if ((node_id == APP_NODE_ID_UNASSIGNED) ||
        (node_id == APP_BROADCAST_ID) ||
        (node_id == APP_COORDINATOR_ID)) {
        return;
    }

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++) {
        if (s_join_table[i].used && (s_join_table[i].node_id == node_id)) {
            if (!s_join_table[i].online) {
                uart1_write_str("INFO: node ");
                uart1_write_u32((uint32_t)node_id);
                uart1_write_str(" reconnected\r\n");
            }
            s_join_table[i].last_seen_ms = now_ms;
            s_join_table[i].online = true;
            return;
        }
    }
}

static void app_check_node_disconnects(uint32_t now_ms)
{
    uint8_t i;

    if (APP_COORDINATOR_MODE == 0U) {
        return;
    }

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++) {
        if (!s_join_table[i].used || !s_join_table[i].online) {
            continue;
        }

        if ((now_ms - s_join_table[i].last_seen_ms) >= APP_NODE_OFFLINE_MS) {
            s_join_table[i].online = false;
            uart1_write_str("WARN: node ");
            uart1_write_u32((uint32_t)s_join_table[i].node_id);
            uart1_write_str(" disconnected\r\n");
        }
    }
}

static void app_print_nodes(void)
{
    uint8_t i;
    bool found = false;
    uint32_t now_ms = platform_millis();

    if (APP_COORDINATOR_MODE == 0U) {
        uart1_write_str("nodes: only available in coordinator mode\r\n");
        return;
    }

    uart1_write_str("Known nodes: ");
    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++) {
        if (!s_join_table[i].used) {
            continue;
        }
        found = true;
        uart1_write_str("[id=");
        uart1_write_u32((uint32_t)s_join_table[i].node_id);
        uart1_write_str(" nonce=");
        uart1_write_u32((uint32_t)s_join_table[i].nonce);
        uart1_write_str(" state=");
        uart1_write_str(s_join_table[i].online ? "online" : "offline");
        uart1_write_str(" ageMs=");
        uart1_write_u32((uint32_t)(now_ms - s_join_table[i].last_seen_ms));
        uart1_write_str("] ");
    }

    if (!found) {
        uart1_write_str("none");
    }
    uart1_write_str("\r\n");
}

static void app_send_ack(uint16_t dst_id, uint16_t ack_seq)
{
    wyresv2_frame_t ack;
    uint32_t now_ms;

    if (s_node_id == APP_NODE_ID_UNASSIGNED) {
        return;
    }

    now_ms = platform_millis();
    app_frame_init(&ack, MSG_ACK, s_node_id, dst_id, ack_seq);
    if (app_send_frame(&ack)) {
        s_ack_tx_count++;
    }

    if ((dst_id != APP_NODE_ID_UNASSIGNED) && (dst_id != APP_BROADCAST_ID)) {
        s_pending_ack_burst.active = true;
        s_pending_ack_burst.dst_id = dst_id;
        s_pending_ack_burst.seq = ack_seq;
        s_pending_ack_burst.repeats_left = APP_ACK_BURST_REPEAT;
        s_pending_ack_burst.next_tx_ms = now_ms + APP_ACK_BURST_DELAY_MS;
    }
}

static void app_process_ack_burst(uint32_t now_ms)
{
    wyresv2_frame_t ack;

    if (!s_pending_ack_burst.active) {
        return;
    }

    if (s_node_id == APP_NODE_ID_UNASSIGNED) {
        s_pending_ack_burst.active = false;
        return;
    }

    if ((int32_t)(now_ms - s_pending_ack_burst.next_tx_ms) < 0) {
        return;
    }

    app_frame_init(&ack,
                   MSG_ACK,
                   s_node_id,
                   s_pending_ack_burst.dst_id,
                   s_pending_ack_burst.seq);
    ack.payload_len = 0U;

    if (!app_send_frame(&ack)) {
        s_pending_ack_burst.next_tx_ms = now_ms + 30U;
        return;
    }

    s_ack_tx_count++;
    s_pending_ack_burst.repeats_left--;
    if (s_pending_ack_burst.repeats_left == 0U) {
        s_pending_ack_burst.active = false;
    } else {
        s_pending_ack_burst.next_tx_ms = now_ms + APP_ACK_BURST_GAP_MS;
    }
}

static void app_pending_clear(void)
{
    memset(&s_pending_tx, 0, sizeof(s_pending_tx));
}

static void app_pending_start(uint16_t dst_id, uint16_t seq, const uint8_t *raw, uint8_t raw_len, uint32_t now_ms)
{
    if ((raw == NULL) || (raw_len == 0U)) {
        return;
    }

    s_pending_tx.active = true;
    s_pending_tx.dst_id = dst_id;
    s_pending_tx.seq = seq;
    s_pending_tx.retries_left = APP_ACK_MAX_RETRIES;
    s_pending_tx.deadline_ms = now_ms + APP_ACK_TIMEOUT_MS;
    s_pending_tx.raw_len = raw_len;
    memcpy(s_pending_tx.raw, raw, raw_len);
}

static void app_pending_process(uint32_t now_ms)
{
    if (!s_pending_tx.active) {
        return;
    }

    if ((int32_t)(now_ms - s_pending_tx.deadline_ms) < 0) {
        return;
    }

    if (s_pending_tx.retries_left == 0U) {
        s_ack_timeout_count++;
        uart1_write_str("ACK TIMEOUT seq=");
        uart1_write_u32((uint32_t)s_pending_tx.seq);
        uart1_write_str(" dst=");
        uart1_write_u32((uint32_t)s_pending_tx.dst_id);
        uart1_write_str("\r\n");
        app_pending_clear();
        return;
    }

    if (app_send_raw(s_pending_tx.raw, s_pending_tx.raw_len)) {
        s_retry_count++;
        s_pending_tx.retries_left--;
        s_pending_tx.deadline_ms = now_ms + APP_ACK_TIMEOUT_MS;
        uart1_write_str("RETRY seq=");
        uart1_write_u32((uint32_t)s_pending_tx.seq);
        uart1_write_str(" left=");
        uart1_write_u32((uint32_t)s_pending_tx.retries_left);
        uart1_write_str("\r\n");
    } else {
        s_pending_tx.deadline_ms = now_ms + APP_RETRY_BACKOFF_MS;
    }
}

static void app_process_ack_frame(const wyresv2_frame_t *frame)
{
    if ((frame == NULL) || !s_pending_tx.active) {
        return;
    }

    if ((frame->src_id == s_pending_tx.dst_id) && (frame->seq == s_pending_tx.seq)) {
        s_ack_rx_count++;
        uart1_write_str("ACK OK seq=");
        uart1_write_u32((uint32_t)frame->seq);
        uart1_write_str(" from=");
        uart1_write_u32((uint32_t)frame->src_id);
        uart1_write_str("\r\n");
        app_pending_clear();
    }
}

static void app_process_join_ack_frame(const wyresv2_frame_t *frame)
{
    uint16_t nonce;
    uint16_t assigned_id;

    if ((frame == NULL) || s_joined || (frame->payload_len < 4U)) {
        return;
    }

    nonce = (uint16_t)frame->payload[0] | ((uint16_t)frame->payload[1] << 8);
    assigned_id = (uint16_t)frame->payload[2] | ((uint16_t)frame->payload[3] << 8);

    if (nonce != s_join_nonce) {
        uart1_write_str("JOIN_ACK ignored nonce=");
        uart1_write_u32((uint32_t)nonce);
        uart1_write_str(" expected=");
        uart1_write_u32((uint32_t)s_join_nonce);
        uart1_write_str("\r\n");
        return;
    }

    if ((assigned_id == APP_NODE_ID_UNASSIGNED) ||
        (assigned_id == APP_BROADCAST_ID)) {
        uart1_write_str("JOIN_ACK ignored invalid id=");
        uart1_write_u32((uint32_t)assigned_id);
        uart1_write_str("\r\n");
        return;
    }

    s_node_id = assigned_id;
    s_joined = true;
    s_default_dst_id = APP_COORDINATOR_ID;
    platform_led_set(LED_INSERTED);

    uart1_write_str("JOINED node_id=");
    uart1_write_u32((uint32_t)s_node_id);
    uart1_write_str("\r\n");
}

static void app_process_join_request_frame(const wyresv2_frame_t *frame)
{
    wyresv2_frame_t reply;
    uint16_t nonce;
    uint16_t assigned_id;
    uint32_t now_ms;

    if ((frame == NULL) || (APP_COORDINATOR_MODE == 0U) || (frame->payload_len < 2U)) {
        return;
    }

    nonce = (uint16_t)frame->payload[0] | ((uint16_t)frame->payload[1] << 8);
    assigned_id = app_join_find_or_assign(nonce);
    if (assigned_id == APP_NODE_ID_UNASSIGNED) {
        return;
    }
    now_ms = platform_millis();
    app_mark_node_seen(assigned_id, now_ms);

    app_frame_init(&reply, MSG_JOIN_ACK, s_node_id, APP_BROADCAST_ID, s_next_seq++);
    reply.payload_len = 4U;
    reply.payload[0] = (uint8_t)(nonce & 0xFFU);
    reply.payload[1] = (uint8_t)((nonce >> 8) & 0xFFU);
    reply.payload[2] = (uint8_t)(assigned_id & 0xFFU);
    reply.payload[3] = (uint8_t)((assigned_id >> 8) & 0xFFU);

    if (app_send_frame(&reply)) {
        s_join_ack_count++;
        uart1_write_str("JOIN_ACK nonce=");
        uart1_write_u32((uint32_t)nonce);
        uart1_write_str(" -> id=");
        uart1_write_u32((uint32_t)assigned_id);
        uart1_write_str("\r\n");
    }
}

static void app_send_join_request(uint32_t now_ms)
{
    wyresv2_frame_t req;
    uint16_t dst_id;

    if (s_joined || (APP_COORDINATOR_MODE != 0U) || (APP_FIXED_NODE_ID != 0U)) {
        return;
    }

    /*
     * Alternate destination between broadcast and direct coordinator:
     * this avoids back-to-back TX busy and improves compatibility.
     */
    dst_id = s_join_req_next_direct ? APP_COORDINATOR_ID : APP_BROADCAST_ID;
    app_frame_init(&req, MSG_JOIN_REQ, APP_NODE_ID_UNASSIGNED, dst_id, s_next_seq++);
    req.payload_len = 2U;
    req.payload[0] = (uint8_t)(s_join_nonce & 0xFFU);
    req.payload[1] = (uint8_t)((s_join_nonce >> 8) & 0xFFU);

    if (app_send_frame(&req)) {
        s_join_req_count++;
        s_last_join_req_ms = now_ms;
        s_join_req_next_direct = !s_join_req_next_direct;
        uart1_write_str("JOIN_REQ nonce=");
        uart1_write_u32((uint32_t)s_join_nonce);
        uart1_write_str(" dst=");
        if (dst_id == APP_BROADCAST_ID) {
            uart1_write_str("broadcast");
        } else {
            uart1_write_u32((uint32_t)dst_id);
        }
        uart1_write_str(" txv=");
        uart1_write_u32((uint32_t)platform_radio_dbg_tx_variant());
        uart1_write_str("\r\n");
    } else {
        uart1_write_str("JOIN_REQ tx busy dst=");
        if (dst_id == APP_BROADCAST_ID) {
            uart1_write_str("broadcast");
        } else {
            uart1_write_u32((uint32_t)dst_id);
        }
        uart1_write_str(" txv=");
        uart1_write_u32((uint32_t)platform_radio_dbg_tx_variant());
        uart1_write_str("\r\n");
    }
}

static void app_send_heartbeat(uint32_t now_ms)
{
    wyresv2_frame_t hb;
    uint16_t dst_id;

    if (!s_joined || (s_node_id == APP_NODE_ID_UNASSIGNED)) {
        return;
    }

    if ((now_ms - s_last_heartbeat_ms) < APP_HEARTBEAT_PERIOD_MS) {
        return;
    }

    dst_id = (APP_COORDINATOR_MODE != 0U) ? APP_BROADCAST_ID : APP_COORDINATOR_ID;
    app_frame_init(&hb, MSG_HEARTBEAT, s_node_id, dst_id, s_next_seq++);
    hb.payload_len = 0U;

    if (app_send_frame(&hb)) {
        s_last_heartbeat_ms = now_ms;
    }
}

static bool app_send_text(uint16_t dst_id, const uint8_t *payload, uint8_t payload_len, uint32_t now_ms, uint16_t *out_seq)
{
    wyresv2_frame_t msg;
    uint8_t raw[11U + WYRESV2_MAX_PAYLOAD];
    uint8_t raw_len;

    if ((payload == NULL) || (payload_len == 0U) || (payload_len > WYRESV2_MAX_PAYLOAD)) {
        return false;
    }

    if (s_node_id == APP_NODE_ID_UNASSIGNED) {
        uart1_write_str("ERR: node not joined yet\r\n");
        return false;
    }

    if ((dst_id != APP_BROADCAST_ID) && s_pending_tx.active) {
        uart1_write_str("ERR: wait ACK before sending next direct msg\r\n");
        return false;
    }

    app_frame_init(&msg, MSG_TEXT, s_node_id, dst_id, s_next_seq++);
    msg.payload_len = payload_len;
    memcpy(msg.payload, payload, payload_len);

    raw_len = app_serialize_frame(&msg, raw, (uint8_t)sizeof(raw));
    if (raw_len == 0U) {
        return false;
    }

    if (!app_send_raw(raw, raw_len)) {
        return false;
    }

    if (out_seq != NULL) {
        *out_seq = msg.seq;
    }

    if (dst_id != APP_BROADCAST_ID) {
        app_pending_start(dst_id, msg.seq, raw, raw_len, now_ms);
    }

    return true;
}

static void app_handle_text_frame(const wyresv2_frame_t *frame, int16_t rssi, int8_t snr)
{
    bool for_me;
    bool duplicate;

    if (frame == NULL) {
        return;
    }

    for_me = (frame->dst_id == APP_BROADCAST_ID) || (frame->dst_id == s_node_id);
    if (!for_me) {
        return;
    }

    duplicate = app_is_duplicate(frame->src_id, frame->seq);

    if (!duplicate) {
        s_text_rx_count++;
        uart1_write_str("RX MSG src=");
        uart1_write_u32((uint32_t)frame->src_id);
        uart1_write_str(" seq=");
        uart1_write_u32((uint32_t)frame->seq);
        uart1_write_str(" RSSI=");
        uart1_write_i32((int32_t)rssi);
        uart1_write_str(" SNR=");
        uart1_write_i32((int32_t)snr);
        uart1_write_str(" \"");
        uart1_write_printable(frame->payload, frame->payload_len);
        uart1_write_str("\"\r\n");
    }

    if ((frame->dst_id != APP_BROADCAST_ID) && (frame->src_id != APP_NODE_ID_UNASSIGNED)) {
        app_send_ack(frame->src_id, frame->seq);
    }
}

static bool app_should_relay_frame(const wyresv2_frame_t *frame)
{
    if (frame == NULL) {
        return false;
    }

    if (frame->ttl == 0U) {
        return false;
    }

    /* Never relay frames that originated from this node (anti-loop). */
    if ((s_node_id != APP_NODE_ID_UNASSIGNED) && (frame->src_id == s_node_id)) {
        return false;
    }

    if (frame->dst_id == s_node_id) {
        return false;
    }

    /*
     * Unicast frames are relayed.
     * Broadcast is relayed for JOIN_REQ/JOIN_ACK to allow multi-hop join.
     */
    if (frame->dst_id == APP_BROADCAST_ID) {
        return (frame->type == MSG_JOIN_ACK) || (frame->type == MSG_JOIN_REQ);
    }

    return true;
}

static void app_relay_frame(const wyresv2_frame_t *frame, link_direction_t from)
{
    wyresv2_frame_t relay;
    uint8_t raw[11U + WYRESV2_MAX_PAYLOAD];
    uint8_t raw_len;
    link_direction_t out_dir;

    if (!app_should_relay_frame(frame)) {
        return;
    }

    /* Relay once per (src,seq) to avoid ping-pong loops. */
    if (app_relay_is_duplicate(frame->src_id, frame->seq)) {
        return;
    }

    relay = *frame;
    relay.ttl--;

    raw_len = app_serialize_frame(&relay, raw, (uint8_t)sizeof(raw));
    if (raw_len == 0U) {
        return;
    }

    out_dir = (from == LINK_PREDECESSOR) ? LINK_SUCCESSOR : LINK_PREDECESSOR;
    if (app_send_raw_dir(out_dir, raw, raw_len)) {
        app_relay_remember(frame->src_id, frame->seq);
        if ((frame->type == MSG_JOIN_REQ) || (frame->type == MSG_JOIN_ACK)) {
            uart1_write_str("RELAY ");
            uart1_write_str((frame->type == MSG_JOIN_REQ) ? "JOIN_REQ" : "JOIN_ACK");
            uart1_write_str(" src=");
            uart1_write_u32((uint32_t)frame->src_id);
            uart1_write_str(" dst=");
            uart1_write_u32((uint32_t)frame->dst_id);
            uart1_write_str(" ttl=");
            uart1_write_u32((uint32_t)relay.ttl);
            uart1_write_str("\r\n");
        }
        return;
    }

    /* Single-radio fallback (dir may be ignored by backend anyway). */
    if (app_send_raw(raw, raw_len)) {
        app_relay_remember(frame->src_id, frame->seq);
        if ((frame->type == MSG_JOIN_REQ) || (frame->type == MSG_JOIN_ACK)) {
            uart1_write_str("RELAY ");
            uart1_write_str((frame->type == MSG_JOIN_REQ) ? "JOIN_REQ" : "JOIN_ACK");
            uart1_write_str(" src=");
            uart1_write_u32((uint32_t)frame->src_id);
            uart1_write_str(" dst=");
            uart1_write_u32((uint32_t)frame->dst_id);
            uart1_write_str(" ttl=");
            uart1_write_u32((uint32_t)relay.ttl);
            uart1_write_str(" (fallback)\r\n");
        }
    }
}

static void app_print_help(void)
{
    uart1_write_str("Commands:\r\n");
    uart1_write_str("  id                -> show current node id\r\n");
    uart1_write_str("  nodes             -> list joined nodes (coordinator only)\r\n");
    uart1_write_str("  status            -> print local radio counters now\r\n");
    uart1_write_str("  dst <id>          -> set default destination id\r\n");
    uart1_write_str("  send <id> <text>  -> send reliable text with ACK/retry\r\n");
    uart1_write_str("  broadcast <text>  -> send broadcast text (no ACK)\r\n");
    uart1_write_str("  join              -> force join request now\r\n");
    uart1_write_str("  help              -> show this help\r\n");
    uart1_write_str("  <text>            -> send to current dst\r\n");
}

static void app_print_identity(void)
{
    uart1_write_str("NODE id=");
    uart1_write_u32((uint32_t)s_node_id);
    uart1_write_str(" joined=");
    uart1_write_str(s_joined ? "yes" : "no");
    uart1_write_str(" dst=");
    uart1_write_u32((uint32_t)s_default_dst_id);
    uart1_write_str(" coordinator=");
    uart1_write_str((APP_COORDINATOR_MODE != 0U) ? "yes" : "no");
    uart1_write_str("\r\n");
}

static void app_send_text_from_line(uint16_t dst_id, char *text, uint32_t now_ms)
{
    uint16_t seq = 0U;
    uint16_t text_len;

    if (text == NULL) {
        return;
    }

    text = skip_spaces(text);
    if (*text == '\0') {
        return;
    }

    text_len = (uint16_t)strlen(text);
    if (text_len > WYRESV2_MAX_PAYLOAD) {
        text_len = WYRESV2_MAX_PAYLOAD;
        uart1_write_str("INFO: text truncated to 64 bytes\r\n");
    }

    if (app_send_text(dst_id, (const uint8_t *)text, (uint8_t)text_len, now_ms, &seq)) {
        uart1_write_str("TX MSG dst=");
        uart1_write_u32((uint32_t)dst_id);
        uart1_write_str(" seq=");
        uart1_write_u32((uint32_t)seq);
        if (dst_id == APP_BROADCAST_ID) {
            uart1_write_str(" (broadcast)");
        }
        uart1_write_str("\r\n");
    } else {
        uart1_write_str("TX FAIL\r\n");
    }
}

static void app_handle_uart_line(char *line, uint32_t now_ms)
{
    char *cursor;
    uint16_t parsed_id;

    if (line == NULL) {
        return;
    }

    cursor = skip_spaces(line);
    rstrip_spaces(cursor);
    if (*cursor == '\0') {
        return;
    }

    if ((strcmp(cursor, "help") == 0) || (strcmp(cursor, "?") == 0)) {
        app_print_help();
        return;
    }

    if (strcmp(cursor, "id") == 0) {
        app_print_identity();
        return;
    }

    if (strcmp(cursor, "nodes") == 0) {
        app_print_nodes();
        return;
    }

    if (strcmp(cursor, "status") == 0) {
        app_print_status(now_ms);
        return;
    }

    if (strcmp(cursor, "join") == 0) {
        app_send_join_request(now_ms);
        return;
    }

    if (strncmp(cursor, "dst", 3U) == 0) {
        cursor += 3;
        if (!parse_u16_token(&cursor, &parsed_id)) {
            uart1_write_str("Usage: dst <id>\r\n");
            return;
        }
        cursor = skip_spaces(cursor);
        if (*cursor != '\0') {
            uart1_write_str("Usage: dst <id>\r\n");
            return;
        }
        s_default_dst_id = parsed_id;
        uart1_write_str("Default dst=");
        uart1_write_u32((uint32_t)s_default_dst_id);
        uart1_write_str("\r\n");
        return;
    }

    if (strncmp(cursor, "send", 4U) == 0) {
        char *msg_ptr;
        cursor += 4;
        if (!parse_u16_token(&cursor, &parsed_id)) {
            uart1_write_str("Usage: send <id> <text>\r\n");
            return;
        }
        msg_ptr = skip_spaces(cursor);
        if (*msg_ptr == '\0') {
            uart1_write_str("Usage: send <id> <text>\r\n");
            return;
        }
        app_send_text_from_line(parsed_id, msg_ptr, now_ms);
        return;
    }

    if (strncmp(cursor, "broadcast", 9U) == 0) {
        char *msg_ptr = skip_spaces(cursor + 9U);
        if (*msg_ptr == '\0') {
            uart1_write_str("Usage: broadcast <text>\r\n");
            return;
        }
        app_send_text_from_line(APP_BROADCAST_ID, msg_ptr, now_ms);
        return;
    }

    app_send_text_from_line(s_default_dst_id, cursor, now_ms);
}

static void app_poll_uart(uint32_t now_ms)
{
    char c;

    while (uart1_try_read_char(&c)) {
        s_uart_last_rx_ms = now_ms;

        if ((c == '\r') || (c == '\n')) {
            if (s_uart_line_len > 0U) {
                s_uart_line[s_uart_line_len] = '\0';
#if (APP_UART_ECHO != 0U)
                uart1_write_str("\r\n");
#endif
                app_handle_uart_line(s_uart_line, now_ms);
                s_uart_line_len = 0U;
                uart1_write_str("> ");
            }
            continue;
        }

        if ((c == '\b') || ((uint8_t)c == 0x7FU)) {
            if (s_uart_line_len > 0U) {
                s_uart_line_len--;
#if (APP_UART_ECHO != 0U)
                uart1_write_str("\b \b");
#endif
            }
            continue;
        }

        if (((uint8_t)c >= 32U) && ((uint8_t)c <= 126U)) {
            if (s_uart_line_len < (APP_UART_LINE_MAX - 1U)) {
                s_uart_line[s_uart_line_len++] = c;
#if (APP_UART_ECHO != 0U)
                uart1_write_char(c);
#endif
            }
        }
    }

    if ((s_uart_line_len > 0U) &&
        ((now_ms - s_uart_last_rx_ms) >= APP_UART_AUTOSUBMIT_MS)) {
        s_uart_line[s_uart_line_len] = '\0';
        app_handle_uart_line(s_uart_line, now_ms);
        s_uart_line_len = 0U;
        uart1_write_str("> ");
    }
}

static void app_radio_rx_cb(link_direction_t from, const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr)
{
    wyresv2_frame_t frame;
    bool parsed;

    (void)from;
    s_rx_count++;

    parsed = app_parse_frame(&frame, data, len);
    if (!parsed || (frame.version != APP_PROTOCOL_VERSION)) {
        uart1_write_str("RX RAW len=");
        uart1_write_u32((uint32_t)len);
        uart1_write_str(" data=\"");
        uart1_write_printable(data, len);
        uart1_write_str("\"\r\n");
        return;
    }

    /*
     * Multi-hop relay:
     * if frame is not for this node (or JOIN_REQ/JOIN_ACK broadcast cases), forward it.
     */
    app_relay_frame(&frame, from);

    if ((APP_COORDINATOR_MODE != 0U) &&
        (frame.src_id != APP_NODE_ID_UNASSIGNED) &&
        (frame.src_id != APP_BROADCAST_ID) &&
        (frame.src_id != APP_COORDINATOR_ID)) {
        app_mark_node_seen(frame.src_id, platform_millis());
    }

    switch (frame.type) {
    case MSG_JOIN_REQ:
        if ((APP_COORDINATOR_MODE != 0U) &&
            ((frame.dst_id == s_node_id) ||
             (frame.dst_id == APP_COORDINATOR_ID) ||
             (frame.dst_id == APP_BROADCAST_ID))) {
            app_process_join_request_frame(&frame);
        }
        break;

    case MSG_JOIN_ACK:
        app_process_join_ack_frame(&frame);
        break;

    case MSG_TEXT:
        app_handle_text_frame(&frame, rssi, snr);
        break;

    case MSG_ACK:
        if ((frame.dst_id == s_node_id) && (s_node_id != APP_NODE_ID_UNASSIGNED)) {
            app_process_ack_frame(&frame);
        }
        break;

    case MSG_HEARTBEAT:
        /* Presence is handled above via src_id tracking. */
        break;

    default:
        break;
    }
}

static void app_periodic(uint32_t now_ms)
{
    if (!s_joined && (APP_COORDINATOR_MODE == 0U) && (APP_FIXED_NODE_ID == 0U)) {
        if ((now_ms - s_last_join_req_ms) >= APP_JOIN_RETRY_MS) {
            app_send_join_request(now_ms);
        }
    }

    app_send_heartbeat(now_ms);
    app_check_node_disconnects(now_ms);
    app_pending_process(now_ms);
    app_process_ack_burst(now_ms);
}

static void app_print_status(uint32_t now_ms)
{
    uint8_t irq = platform_radio_dbg_last_irq_flags();
    uint8_t opm = platform_radio_dbg_last_opmode();

    uart1_write_str("ALIVE t=");
    uart1_write_u32(now_ms);
    uart1_write_str("ms ID=");
    uart1_write_u32((uint32_t)s_node_id);
    uart1_write_str(" TX=");
    uart1_write_u32(s_tx_count);
    uart1_write_str(" RX=");
    uart1_write_u32(s_rx_count);
    uart1_write_str(" TXT=");
    uart1_write_u32(s_text_rx_count);
    uart1_write_str(" ACKtx=");
    uart1_write_u32(s_ack_tx_count);
    uart1_write_str(" ACKrx=");
    uart1_write_u32(s_ack_rx_count);
    uart1_write_str(" AB=");
    uart1_write_u32((uint32_t)(s_pending_ack_burst.active ? 1U : 0U));
    uart1_write_str(" RTY=");
    uart1_write_u32(s_retry_count);
    uart1_write_str(" ATO=");
    uart1_write_u32(s_ack_timeout_count);
    uart1_write_str(" JOINreq=");
    uart1_write_u32(s_join_req_count);
    uart1_write_str(" JOINack=");
    uart1_write_u32(s_join_ack_count);
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
    uart1_write_hex8(irq);
    uart1_write_str(" OPM=0x");
    uart1_write_hex8(opm);
    uart1_write_str(" TV=");
    uart1_write_u32((uint32_t)platform_radio_dbg_tx_variant());
    uart1_write_str(" BAT=");
    uart1_write_u32(platform_battery_mv());
    uart1_write_str("mV\r\n");
}

static void app_init_identity(void)
{
    memset(&s_pending_tx, 0, sizeof(s_pending_tx));
    memset(&s_pending_ack_burst, 0, sizeof(s_pending_ack_burst));
    memset(s_rx_track, 0, sizeof(s_rx_track));
    memset(s_relay_track, 0, sizeof(s_relay_track));
    memset(s_join_table, 0, sizeof(s_join_table));

    s_next_seq = 1U;
    s_join_nonce = app_read_uid_nonce();
    s_last_join_req_ms = 0U;
    s_last_heartbeat_ms = 0U;
    s_next_assigned_id = APP_JOIN_FIRST_ASSIGN_ID;
    s_relay_track_insert_idx = 0U;
    s_join_req_next_direct = false;

    if (APP_FIXED_NODE_ID != 0U) {
        s_node_id = APP_FIXED_NODE_ID;
        s_joined = true;
    } else if (APP_COORDINATOR_MODE != 0U) {
        s_node_id = APP_COORDINATOR_ID;
        s_joined = true;
    } else {
        s_node_id = APP_NODE_ID_UNASSIGNED;
        s_joined = false;
    }

    s_default_dst_id = APP_COORDINATOR_ID;
    if ((APP_COORDINATOR_MODE != 0U) && (s_node_id == APP_COORDINATOR_ID)) {
        s_default_dst_id = APP_BROADCAST_ID;
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
    uint32_t now_ms;
    uint32_t last_periodic_ms = 0xFFFFFFFFUL;
    bool radio_ok;

    system_clock_init_hsi_16mhz();
    systick_init_1khz();

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
    app_init_identity();

    uart1_write_str("BOOT WYRESV2 STM32L151\r\n");
    uart1_write_str("BUILD: JOIN_UART_IRQ_V6\r\n");
    uart1_write_str("MODE: LoRa text + ID join + ACK retransmission\r\n");
    uart1_write_str("cfg coordinator=");
    uart1_write_str((APP_COORDINATOR_MODE != 0U) ? "1" : "0");
    uart1_write_str(" fixedId=");
    uart1_write_u32((uint32_t)APP_FIXED_NODE_ID);
    uart1_write_str(" joinNonce=");
    uart1_write_u32((uint32_t)s_join_nonce);
    uart1_write_str("\r\n");

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
            uart1_write_hex8(platform_radio_probe_version(probe_i));
            uart1_write_str("\r\n");
        }
    }

    if (s_joined) {
        platform_led_set(LED_INSERTED);
        uart1_write_str("Node ready with id=");
        uart1_write_u32((uint32_t)s_node_id);
        uart1_write_str("\r\n");
    } else {
        platform_led_set(LED_NEUTRAL);
        uart1_write_str("Node waiting for JOIN_ACK from coordinator id=");
        uart1_write_u32((uint32_t)APP_COORDINATOR_ID);
        uart1_write_str("\r\n");
    }

    app_print_help();
    uart1_write_str("> ");

    while (1) {
        now_ms = s_system_ms;
        platform_port_set_time_ms(now_ms);
        platform_radio_process();

        if (radio_ok) {
            app_poll_uart(now_ms);
            if (now_ms != last_periodic_ms) {
                app_periodic(now_ms);
                last_periodic_ms = now_ms;
            }
        }
    }
}

