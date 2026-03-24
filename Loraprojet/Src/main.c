#include "main.h"
#include "radio.h"
#include "radio_board_if.h"
#include "subghz.h"
#include "timer.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define RF_FREQUENCY_HZ        868000000U
#define TX_OUTPUT_POWER_DBM    14
#define LORA_BANDWIDTH         0
#define LORA_SPREADING_FACTOR  7
#define LORA_CODINGRATE        1
#define LORA_PREAMBLE_LENGTH   8
#define LORA_SYMBOL_TIMEOUT    5
#define TX_TIMEOUT_MS          3000U
#define APP_TX_RECOVER_MS      1200U
#define APP_RADIO_IRQ_IN_ISR   1U

#define APP_PROTOCOL_VERSION   1U
#define APP_COORDINATOR_ID     1U
#define APP_NODE_UNASSIGNED    0U
#define APP_BROADCAST_ID       0xFFFFU
#define APP_MAX_PAYLOAD        64U
#define APP_FRAME_OVERHEAD     11U
#define APP_MAX_FRAME_LEN      (APP_FRAME_OVERHEAD + APP_MAX_PAYLOAD)
#define APP_TX_QUEUE_SIZE      8U
#define APP_UART_LINE_MAX      96U
#define APP_RELAY_TRACK_SIZE   24U
#define APP_JOIN_TABLE_SIZE    16U
#define APP_JOIN_FIRST_ID      2U
#define APP_JOIN_LAST_ID       250U
#define APP_JOIN_ACK_REPEAT    3U
#define APP_JOIN_ACK_DELAY_MS  120U
#define APP_JOIN_ACK_GAP_MS    120U
#define APP_NODE_OFFLINE_MS    30000U
#define APP_ACK_TIMEOUT_MS     700U
#define APP_ACK_MAX_RETRIES    3U
#define APP_ACK_BURST_REPEAT   2U
#define APP_ACK_BURST_DELAY_MS 80U
#define APP_ACK_BURST_GAP_MS   80U
/* 2 = USART2 (PA2/PA3, usually ST-LINK VCP), 1 = USART1 (PB6/PB7) */
#define APP_CONSOLE_UART       2U
#define APP_UART_MIRROR_BOTH   1U
#define APP_UART_ECHO          0U
#define APP_UART_AUTOSUBMIT_MS 900U
#define APP_UART_RX_RING_SIZE  256U

typedef enum
{
    APP_MSG_TEXT = 1,
    APP_MSG_ALERT = 2,
    APP_MSG_SUPERVISION = 3,
    APP_MSG_ACK = 4,
    APP_MSG_JOIN_REQ = 5,
    APP_MSG_JOIN_ACK = 6,
    APP_MSG_HEARTBEAT = 7
} app_msg_type_t;

typedef struct
{
    uint8_t version;
    uint8_t type;
    uint16_t src_id;
    uint16_t dst_id;
    uint16_t seq;
    uint8_t ttl;
    uint8_t flags;
    uint8_t payload_len;
    uint8_t payload[APP_MAX_PAYLOAD];
} app_frame_t;

typedef struct
{
    uint8_t used;
    uint16_t nonce;
    uint16_t node_id;
    uint32_t last_seen_ms;
    uint8_t online;
} app_join_entry_t;

typedef struct
{
    uint8_t len;
    uint8_t data[255];
} app_tx_item_t;

typedef struct
{
    uint8_t active;
    uint16_t dst_id;
    uint16_t seq;
    uint8_t retries_left;
    uint8_t raw_len;
    uint32_t deadline_ms;
    uint8_t raw[APP_MAX_FRAME_LEN];
} app_pending_ack_t;

typedef struct
{
    uint8_t used;
    uint16_t src_id;
    uint16_t seq;
} app_relay_track_t;

typedef struct
{
    uint8_t active;
    uint16_t nonce;
    uint16_t assigned_id;
    uint8_t repeats_left;
    uint32_t next_tx_ms;
} app_join_ack_burst_t;

typedef struct
{
    uint8_t active;
    uint16_t dst_id;
    uint16_t seq;
    uint8_t repeats_left;
    uint32_t next_tx_ms;
} app_ack_burst_t;

typedef struct
{
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t overflow_count;
    uint8_t data[APP_UART_RX_RING_SIZE];
} app_uart_ring_t;

static UART_HandleTypeDef huart1;
static UART_HandleTypeDef huart2;
static uint8_t uart1_ready = 0U;
static uint8_t uart2_ready = 0U;

static RadioEvents_t radio_events;

static volatile uint8_t radio_tx_done = 0U;
static volatile uint8_t radio_tx_timeout = 0U;
static volatile uint8_t radio_rx_done = 0U;
static volatile uint8_t radio_rx_timeout = 0U;
static volatile uint8_t radio_rx_error = 0U;

static uint8_t rx_buffer[255];
static uint16_t rx_size = 0U;
static int16_t rx_last_rssi = 0;
static int8_t rx_last_snr = 0;

static uint8_t tx_pending = 0U;
static uint32_t tx_start_ms = 0U;

static app_tx_item_t tx_queue[APP_TX_QUEUE_SIZE];
static uint8_t tx_queue_head = 0U;
static uint8_t tx_queue_tail = 0U;
static uint8_t tx_queue_count = 0U;

static app_pending_ack_t pending_ack;
static app_join_ack_burst_t pending_join_ack;
static app_ack_burst_t pending_ack_burst;
static app_relay_track_t relay_track[APP_RELAY_TRACK_SIZE];
static app_join_entry_t join_table[APP_JOIN_TABLE_SIZE];
static uint16_t next_assigned_id = APP_JOIN_FIRST_ID;
static uint16_t next_seq = 1U;
static uint16_t default_dst_id = APP_BROADCAST_ID;
static uint8_t uart_rx_seen = 0U;
static uint32_t uart_last_rx_char_ms = 0U;
static uint8_t uart_last_read_src = 0U;
static app_uart_ring_t uart1_rx_ring;
static app_uart_ring_t uart2_rx_ring;

static char uart_line[APP_UART_LINE_MAX];
static uint8_t uart_line_len = 0U;
static uint8_t uart_line_src = 0U; /* 0=none, 1=USART1, 2=USART2 */
static uint8_t relay_track_insert_idx = 0U;

static uint32_t stat_rx_total = 0U;
static uint32_t stat_tx_started = 0U;
static uint32_t stat_tx_done = 0U;
static uint32_t stat_tx_timeout = 0U;
static uint32_t stat_tx_fail = 0U;
static uint32_t stat_tx_recover = 0U;
static uint32_t stat_parse_err = 0U;
static uint32_t stat_join_req = 0U;
static uint32_t stat_join_ack = 0U;
static uint32_t stat_text_rx = 0U;
static uint32_t stat_ack_tx = 0U;
static uint32_t stat_ack_rx = 0U;
static uint32_t stat_ack_timeout = 0U;
static uint32_t stat_retry = 0U;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART_UART_Init(void);
static void Radio_Init(void);
static void Radio_Reapply_Config(void);

static void OnTxDone(void);
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnTxTimeout(void);
static void OnRxTimeout(void);
static void OnRxError(void);

static void Uart_TransmitAll(const uint8_t *buf, uint16_t len);
static void Uart_Log(const char *msg);
static void Uart_Logf(const char *fmt, ...);
static void Uart_LogTimedf(const char *fmt, ...);
static uint8_t Uart_TryReadChar(char *out_char);
static uint8_t Uart_TryReadFrom(UART_HandleTypeDef *huart, char *out_char);
static void Uart_RingReset(app_uart_ring_t *ring);
static void Uart_RingPush(app_uart_ring_t *ring, uint8_t value);
static uint8_t Uart_RingPop(app_uart_ring_t *ring, char *out_char);
static void Uart_IrqDrain(UART_HandleTypeDef *huart, app_uart_ring_t *ring);

static char *SkipSpaces(char *p);
static uint8_t ParseU16(char **cursor, uint16_t *out_value);

static uint8_t App_SerializeFrame(const app_frame_t *frame, uint8_t *out, uint8_t out_max);
static uint8_t App_ParseFrame(app_frame_t *frame, const uint8_t *raw, uint16_t raw_len);
static void App_InitFrame(app_frame_t *frame, uint8_t type, uint16_t src_id, uint16_t dst_id, uint16_t seq);
static uint8_t App_QueueRaw(const uint8_t *data, uint8_t len);
static uint8_t App_TxQueuePeek(app_tx_item_t *out_item);
static void App_TxQueuePop(void);
static uint8_t App_RadioSendRaw(const uint8_t *data, uint8_t len);
static void App_TxPump(void);
static uint8_t App_QueueFrame(const app_frame_t *frame);

static uint8_t App_JoinIdInUse(uint16_t node_id);
static uint16_t App_JoinAllocateId(void);
static uint16_t App_JoinFindOrAssign(uint16_t nonce);
static void App_MarkNodeSeen(uint16_t node_id, uint32_t now_ms);
static void App_CheckNodeDisconnects(uint32_t now_ms);
static uint8_t App_RelayIsDuplicate(uint16_t src_id, uint16_t seq);
static void App_RelayRemember(uint16_t src_id, uint16_t seq);
static void App_RelayFrame(const app_frame_t *frame);
static void App_PrintNodes(void);
static void App_PrintStatus(void);

static void App_StartPendingAck(uint16_t dst_id, uint16_t seq, const uint8_t *raw, uint8_t raw_len, uint32_t now_ms);
static void App_ProcessPendingAck(uint32_t now_ms);
static void App_ProcessAckFrame(const app_frame_t *frame);
static void App_ScheduleJoinAck(uint16_t nonce, uint16_t assigned_id, uint32_t now_ms);
static void App_ProcessJoinAckBurst(uint32_t now_ms);
static void App_ScheduleAckBurst(uint16_t dst_id, uint16_t seq, uint32_t now_ms);
static void App_ProcessAckBurst(uint32_t now_ms);

static void App_HandleJoinReq(const app_frame_t *frame);
static void App_HandleText(const app_frame_t *frame, int16_t rssi, int8_t snr);
static void App_HandleFrame(const app_frame_t *frame, int16_t rssi, int8_t snr);
static void App_HandleRxRaw(const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr);

static void App_SendText(uint16_t dst_id, const char *text, uint32_t now_ms);
static void App_PrintHelp(void);
static void App_HandleUartLine(char *line, uint32_t now_ms);
static void App_SubmitUartLine(uint32_t now_ms);
static void App_PollUart(uint32_t now_ms);

int main(void)
{
    uint32_t now_ms;

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART_UART_Init();
    MX_SUBGHZ_Init();

    memset(&pending_ack, 0, sizeof(pending_ack));
    memset(&pending_join_ack, 0, sizeof(pending_join_ack));
    memset(&pending_ack_burst, 0, sizeof(pending_ack_burst));
    memset(relay_track, 0, sizeof(relay_track));
    memset(join_table, 0, sizeof(join_table));
    relay_track_insert_idx = 0U;

    Uart_Log("BOOT LORAPROJET\r\n");
    Uart_Log("MODE: CARD1 COORDINATOR (LORAE5)\r\n");
    Uart_Log("IRQ MODE: HYBRID_ISR_POLL\r\n");
    Uart_Log("ACK BURST: ON\r\n");
    Uart_Log("BUILD: " __DATE__ " " __TIME__ "\r\n");
#if (APP_CONSOLE_UART == 2U)
    Uart_Log("UART CONSOLE: USART2 (PA2/PA3)\r\n");
#else
    Uart_Log("UART CONSOLE: USART1 (PB6/PB7)\r\n");
#endif
    Uart_Log("UART RX MODE: IRQ_RING + PER_LINE_SOURCE\r\n");
    Uart_Log("RFSW PROFILE ID: " STR(RBI_RF_SW_PROFILE) "\r\n");
#if (RBI_RF_SW_PROFILE == RBI_RF_SW_PROFILE_WYRES_REVC)
    Uart_Log("RFSW PROFILE: WYRES_REVC\r\n");
#else
    Uart_Log("RFSW PROFILE: LORAE5\r\n");
#endif
    Uart_Log("COORDINATOR NODE ID: 1\r\n");
    Uart_Logf("PRESENCE OFFLINE THRESHOLD: %lu ms\r\n",
              (unsigned long)APP_NODE_OFFLINE_MS);
    Uart_Log("SERIAL: type text + Enter (or short pause) to send on LoRa\r\n");
    Uart_Log("SERIAL: commands available via help\r\n");

    Radio_Init();
    Radio.SetPublicNetwork(false);
    Radio.Rx(0);

    App_PrintHelp();
    Uart_Log("> ");

    while (1)
    {
        now_ms = HAL_GetTick();

        /*
         * Keep polling IrqProcess even in ISR mode.
         * Some boards/firmware revisions occasionally miss radio IRQ delivery;
         * this hybrid path prevents silent RX/TX callback starvation.
         */
        Radio.IrqProcess();
        TimerProcess();

        if (radio_rx_done != 0U)
        {
            radio_rx_done = 0U;
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            App_HandleRxRaw(rx_buffer, rx_size, rx_last_rssi, rx_last_snr);
            Radio.Rx(0);
        }

        if (radio_tx_done != 0U)
        {
            radio_tx_done = 0U;
            tx_pending = 0U;
            stat_tx_done++;
            Radio.Rx(0);
        }

        if (radio_tx_timeout != 0U)
        {
            radio_tx_timeout = 0U;
            tx_pending = 0U;
            stat_tx_timeout++;
            Radio.Rx(0);
        }

        if (radio_rx_timeout != 0U)
        {
            radio_rx_timeout = 0U;
            Radio.Rx(0);
        }

        if (radio_rx_error != 0U)
        {
            radio_rx_error = 0U;
            Radio.Rx(0);
        }

        if (tx_pending != 0U)
        {
            RadioState_t st = Radio.GetStatus();
            if (st != RF_TX_RUNNING)
            {
                tx_pending = 0U;
                stat_tx_recover++;
                Radio.Rx(0);
            }
            else if ((now_ms - tx_start_ms) > APP_TX_RECOVER_MS)
            {
                tx_pending = 0U;
                stat_tx_recover++;
                Radio.Standby();
                Radio_Reapply_Config();
                Radio.Rx(0);
            }
        }

        App_PollUart(now_ms);
        App_ProcessPendingAck(now_ms);
        App_CheckNodeDisconnects(now_ms);
        App_ProcessJoinAckBurst(now_ms);
        App_ProcessAckBurst(now_ms);
        App_TxPump();
    }
}

static char *SkipSpaces(char *p)
{
    if (p == NULL)
    {
        return NULL;
    }

    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }
    return p;
}

static uint8_t ParseU16(char **cursor, uint16_t *out_value)
{
    char *p;
    uint32_t value = 0U;
    uint8_t has_digit = 0U;

    if ((cursor == NULL) || (*cursor == NULL) || (out_value == NULL))
    {
        return 0U;
    }

    p = SkipSpaces(*cursor);
    while ((*p >= '0') && (*p <= '9'))
    {
        has_digit = 1U;
        value = (value * 10U) + (uint32_t)(*p - '0');
        if (value > 65535U)
        {
            return 0U;
        }
        p++;
    }

    if (has_digit == 0U)
    {
        return 0U;
    }

    *out_value = (uint16_t)value;
    *cursor = p;
    return 1U;
}

static uint8_t App_SerializeFrame(const app_frame_t *frame, uint8_t *out, uint8_t out_max)
{
    uint8_t need;
    uint8_t i;

    if ((frame == NULL) || (out == NULL))
    {
        return 0U;
    }

    if (frame->payload_len > APP_MAX_PAYLOAD)
    {
        return 0U;
    }

    need = (uint8_t)(APP_FRAME_OVERHEAD + frame->payload_len);
    if (need > out_max)
    {
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

    for (i = 0U; i < frame->payload_len; i++)
    {
        out[APP_FRAME_OVERHEAD + i] = frame->payload[i];
    }

    return need;
}

static uint8_t App_ParseFrame(app_frame_t *frame, const uint8_t *raw, uint16_t raw_len)
{
    uint8_t i;
    uint8_t payload_len;
    uint16_t need;

    if ((frame == NULL) || (raw == NULL) || (raw_len < APP_FRAME_OVERHEAD))
    {
        return 0U;
    }

    payload_len = raw[10];
    if (payload_len > APP_MAX_PAYLOAD)
    {
        return 0U;
    }

    need = (uint16_t)APP_FRAME_OVERHEAD + (uint16_t)payload_len;
    if (raw_len != need)
    {
        return 0U;
    }

    frame->version = raw[0];
    frame->type = raw[1];
    frame->src_id = (uint16_t)raw[2] | ((uint16_t)raw[3] << 8);
    frame->dst_id = (uint16_t)raw[4] | ((uint16_t)raw[5] << 8);
    frame->seq = (uint16_t)raw[6] | ((uint16_t)raw[7] << 8);
    frame->ttl = raw[8];
    frame->flags = raw[9];
    frame->payload_len = payload_len;
    for (i = 0U; i < payload_len; i++)
    {
        frame->payload[i] = raw[APP_FRAME_OVERHEAD + i];
    }

    return 1U;
}

static void App_InitFrame(app_frame_t *frame, uint8_t type, uint16_t src_id, uint16_t dst_id, uint16_t seq)
{
    if (frame == NULL)
    {
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

static uint8_t App_QueueRaw(const uint8_t *data, uint8_t len)
{
    app_tx_item_t *slot;

    if ((data == NULL) || (len == 0U) || (len > sizeof(tx_queue[0].data)))
    {
        return 0U;
    }

    if (tx_queue_count >= APP_TX_QUEUE_SIZE)
    {
        return 0U;
    }

    slot = &tx_queue[tx_queue_tail];
    slot->len = len;
    memcpy(slot->data, data, len);
    tx_queue_tail++;
    if (tx_queue_tail >= APP_TX_QUEUE_SIZE)
    {
        tx_queue_tail = 0U;
    }
    tx_queue_count++;
    return 1U;
}

static uint8_t App_TxQueuePeek(app_tx_item_t *out_item)
{
    if ((out_item == NULL) || (tx_queue_count == 0U))
    {
        return 0U;
    }

    *out_item = tx_queue[tx_queue_head];
    return 1U;
}

static void App_TxQueuePop(void)
{
    if (tx_queue_count == 0U)
    {
        return;
    }

    tx_queue_head++;
    if (tx_queue_head >= APP_TX_QUEUE_SIZE)
    {
        tx_queue_head = 0U;
    }
    tx_queue_count--;
}

static uint8_t App_RadioSendRaw(const uint8_t *data, uint8_t len)
{
    radio_status_t st;

    if ((data == NULL) || (len == 0U) || (tx_pending != 0U))
    {
        return 0U;
    }

    Radio.Standby();
    st = Radio.Send((uint8_t *)data, len);
    if (st != RADIO_STATUS_OK)
    {
        stat_tx_fail++;
        return 0U;
    }

    tx_pending = 1U;
    tx_start_ms = HAL_GetTick();
    stat_tx_started++;
    return 1U;
}

static void App_TxPump(void)
{
    app_tx_item_t item;

    if (tx_pending != 0U)
    {
        return;
    }

    if (!App_TxQueuePeek(&item))
    {
        return;
    }

    if (App_RadioSendRaw(item.data, item.len))
    {
        App_TxQueuePop();
    }
}

static uint8_t App_QueueFrame(const app_frame_t *frame)
{
    uint8_t raw[APP_MAX_FRAME_LEN];
    uint8_t raw_len;

    raw_len = App_SerializeFrame(frame, raw, (uint8_t)sizeof(raw));
    if (raw_len == 0U)
    {
        return 0U;
    }

    return App_QueueRaw(raw, raw_len);
}

static uint8_t App_JoinIdInUse(uint16_t node_id)
{
    uint8_t i;

    if ((node_id == APP_NODE_UNASSIGNED) || (node_id == APP_BROADCAST_ID) || (node_id == APP_COORDINATOR_ID))
    {
        return 1U;
    }

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++)
    {
        if (join_table[i].used && (join_table[i].node_id == node_id))
        {
            return 1U;
        }
    }

    return 0U;
}

static uint16_t App_JoinAllocateId(void)
{
    uint16_t candidate = next_assigned_id;
    uint16_t tries = (uint16_t)(APP_JOIN_LAST_ID - APP_JOIN_FIRST_ID + 1U);

    while (tries-- != 0U)
    {
        if ((candidate < APP_JOIN_FIRST_ID) || (candidate > APP_JOIN_LAST_ID))
        {
            candidate = APP_JOIN_FIRST_ID;
        }

        if (!App_JoinIdInUse(candidate))
        {
            next_assigned_id = (uint16_t)(candidate + 1U);
            return candidate;
        }

        candidate++;
    }

    return APP_NODE_UNASSIGNED;
}

static uint16_t App_JoinFindOrAssign(uint16_t nonce)
{
    uint8_t i;
    int8_t free_idx = -1;
    uint16_t id;
    uint32_t now_ms = HAL_GetTick();

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++)
    {
        if (join_table[i].used)
        {
            if (join_table[i].nonce == nonce)
            {
                App_MarkNodeSeen(join_table[i].node_id, now_ms);
                return join_table[i].node_id;
            }
        }
        else if (free_idx < 0)
        {
            free_idx = (int8_t)i;
        }
    }

    if (free_idx < 0)
    {
        return APP_NODE_UNASSIGNED;
    }

    id = App_JoinAllocateId();
    if (id == APP_NODE_UNASSIGNED)
    {
        return APP_NODE_UNASSIGNED;
    }

    join_table[(uint8_t)free_idx].used = 1U;
    join_table[(uint8_t)free_idx].nonce = nonce;
    join_table[(uint8_t)free_idx].node_id = id;
    join_table[(uint8_t)free_idx].last_seen_ms = now_ms;
    join_table[(uint8_t)free_idx].online = 1U;
    return id;
}

static void App_MarkNodeSeen(uint16_t node_id, uint32_t now_ms)
{
    uint8_t i;
    uint8_t found = 0U;
    uint8_t was_online = 0U;
    uint8_t stale_seen_while_online = 0U;

    if ((node_id == APP_NODE_UNASSIGNED) ||
        (node_id == APP_BROADCAST_ID) ||
        (node_id == APP_COORDINATOR_ID))
    {
        return;
    }

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++)
    {
        if (join_table[i].used && (join_table[i].node_id == node_id))
        {
            found = 1U;
            if (join_table[i].online != 0U)
            {
                was_online = 1U;
                if ((now_ms >= join_table[i].last_seen_ms) &&
                    ((now_ms - join_table[i].last_seen_ms) >= APP_NODE_OFFLINE_MS))
                {
                    stale_seen_while_online = 1U;
                }
            }
            join_table[i].last_seen_ms = now_ms;
            join_table[i].online = 1U;
        }
    }

    /*
     * Safety net:
     * if a long silence is observed while node was still marked online,
     * emit the disconnect event before the reconnect event.
     */
    if ((found != 0U) && (stale_seen_while_online != 0U))
    {
        Uart_LogTimedf("WARN: node %u disconnected\r\n", (unsigned)node_id);
    }

    if ((found != 0U) && ((was_online == 0U) || (stale_seen_while_online != 0U)))
    {
        Uart_LogTimedf("INFO: node %u reconnected\r\n", (unsigned)node_id);
    }
}

static void App_CheckNodeDisconnects(uint32_t now_ms)
{
    uint8_t i;
    uint8_t j;

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++)
    {
        uint16_t node_id;
        uint32_t latest_seen_ms = 0U;
        uint8_t any_online = 0U;
        uint8_t already_processed = 0U;

        if (!join_table[i].used)
        {
            continue;
        }

        node_id = join_table[i].node_id;
        if ((node_id == APP_NODE_UNASSIGNED) ||
            (node_id == APP_BROADCAST_ID) ||
            (node_id == APP_COORDINATOR_ID))
        {
            continue;
        }

        for (j = 0U; j < i; j++)
        {
            if (join_table[j].used && (join_table[j].node_id == node_id))
            {
                already_processed = 1U;
                break;
            }
        }
        if (already_processed != 0U)
        {
            continue;
        }

        for (j = 0U; j < APP_JOIN_TABLE_SIZE; j++)
        {
            if (!join_table[j].used || (join_table[j].node_id != node_id))
            {
                continue;
            }

            if (join_table[j].online != 0U)
            {
                any_online = 1U;
            }

            if ((latest_seen_ms == 0U) || (join_table[j].last_seen_ms > latest_seen_ms))
            {
                latest_seen_ms = join_table[j].last_seen_ms;
            }
        }

        if ((any_online == 0U) || (latest_seen_ms == 0U))
        {
            continue;
        }

        /* Guard against incoherent future timestamps (underflow would look like huge age). */
        if (latest_seen_ms > now_ms)
        {
            latest_seen_ms = now_ms;
        }

        if ((now_ms - latest_seen_ms) >= APP_NODE_OFFLINE_MS)
        {
            for (j = 0U; j < APP_JOIN_TABLE_SIZE; j++)
            {
                if (join_table[j].used && (join_table[j].node_id == node_id))
                {
                    join_table[j].online = 0U;
                    join_table[j].last_seen_ms = latest_seen_ms;
                }
            }
            Uart_LogTimedf("WARN: node %u disconnected\r\n", (unsigned)node_id);
        }
    }
}

static uint8_t App_RelayIsDuplicate(uint16_t src_id, uint16_t seq)
{
    uint8_t i;

    for (i = 0U; i < APP_RELAY_TRACK_SIZE; i++)
    {
        if (relay_track[i].used &&
            (relay_track[i].src_id == src_id) &&
            (relay_track[i].seq == seq))
        {
            return 1U;
        }
    }

    return 0U;
}

static void App_RelayRemember(uint16_t src_id, uint16_t seq)
{
    app_relay_track_t *slot = &relay_track[relay_track_insert_idx];

    slot->used = 1U;
    slot->src_id = src_id;
    slot->seq = seq;
    relay_track_insert_idx++;
    if (relay_track_insert_idx >= APP_RELAY_TRACK_SIZE)
    {
        relay_track_insert_idx = 0U;
    }
}

static void App_RelayFrame(const app_frame_t *frame)
{
    app_frame_t relay;

    if (frame == NULL)
    {
        return;
    }

    if (frame->ttl == 0U)
    {
        return;
    }

    if (frame->dst_id == APP_COORDINATOR_ID)
    {
        return;
    }

    if (frame->dst_id == APP_BROADCAST_ID)
    {
        return;
    }

    /* Anti-loop: never relay a frame that originated from this coordinator. */
    if (frame->src_id == APP_COORDINATOR_ID)
    {
        return;
    }

    if (App_RelayIsDuplicate(frame->src_id, frame->seq))
    {
        return;
    }

    relay = *frame;
    relay.ttl--;
    if (App_QueueFrame(&relay))
    {
        App_RelayRemember(frame->src_id, frame->seq);
        Uart_LogTimedf("RELAY src=%u dst=%u seq=%u ttl=%u\r\n",
                       (unsigned)frame->src_id,
                       (unsigned)frame->dst_id,
                       (unsigned)frame->seq,
                       (unsigned)relay.ttl);
    }
}

static void App_PrintNodes(void)
{
    uint8_t i;
    uint8_t found = 0U;
    uint32_t now_ms = HAL_GetTick();
    uint32_t age_ms;

    Uart_Log("Known nodes: ");
    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++)
    {
        if (join_table[i].used)
        {
            found = 1U;
            if (join_table[i].last_seen_ms <= now_ms)
            {
                age_ms = now_ms - join_table[i].last_seen_ms;
            }
            else
            {
                age_ms = 0U;
            }
            Uart_Logf("[id=%u nonce=%u state=%s ageMs=%lu] ",
                      (unsigned)join_table[i].node_id,
                      (unsigned)join_table[i].nonce,
                      (join_table[i].online != 0U) ? "online" : "offline",
                      (unsigned long)age_ms);
        }
    }
    if (found == 0U)
    {
        Uart_Log("none");
    }
    Uart_Log("\r\n");
}

static void App_PrintStatus(void)
{
    RadioState_t st = Radio.GetStatus();

    Uart_Logf("STATUS rx=%lu txStart=%lu txDone=%lu txTo=%lu txFail=%lu txRec=%lu parseErr=%lu\r\n",
              (unsigned long)stat_rx_total,
              (unsigned long)stat_tx_started,
              (unsigned long)stat_tx_done,
              (unsigned long)stat_tx_timeout,
              (unsigned long)stat_tx_fail,
              (unsigned long)stat_tx_recover,
              (unsigned long)stat_parse_err);
    Uart_Logf("STATUS joinReq=%lu joinAck=%lu textRx=%lu ackTx=%lu ackRx=%lu ackTo=%lu retry=%lu\r\n",
              (unsigned long)stat_join_req,
              (unsigned long)stat_join_ack,
              (unsigned long)stat_text_rx,
              (unsigned long)stat_ack_tx,
              (unsigned long)stat_ack_rx,
              (unsigned long)stat_ack_timeout,
              (unsigned long)stat_retry);
    Uart_Logf("STATUS queue=%u pendingAck=%u pendingAckBurst=%u pendingJoinAck=%u txPending=%u radioSt=%u\r\n",
              (unsigned)tx_queue_count,
              (unsigned)pending_ack.active,
              (unsigned)pending_ack_burst.active,
              (unsigned)pending_join_ack.active,
              (unsigned)tx_pending,
              (unsigned)st);
    Uart_Logf("STATUS uartLineSrc=%u uart1Ov=%lu uart2Ov=%lu\r\n",
              (unsigned)uart_line_src,
              (unsigned long)uart1_rx_ring.overflow_count,
              (unsigned long)uart2_rx_ring.overflow_count);
}

static void App_StartPendingAck(uint16_t dst_id, uint16_t seq, const uint8_t *raw, uint8_t raw_len, uint32_t now_ms)
{
    if ((raw == NULL) || (raw_len == 0U))
    {
        return;
    }

    pending_ack.active = 1U;
    pending_ack.dst_id = dst_id;
    pending_ack.seq = seq;
    pending_ack.retries_left = APP_ACK_MAX_RETRIES;
    pending_ack.raw_len = raw_len;
    pending_ack.deadline_ms = now_ms + APP_ACK_TIMEOUT_MS;
    memcpy(pending_ack.raw, raw, raw_len);
}

static void App_ProcessPendingAck(uint32_t now_ms)
{
    if (pending_ack.active == 0U)
    {
        return;
    }

    if ((int32_t)(now_ms - pending_ack.deadline_ms) < 0)
    {
        return;
    }

    if (pending_ack.retries_left == 0U)
    {
        pending_ack.active = 0U;
        stat_ack_timeout++;
        Uart_Logf("ACK TIMEOUT seq=%u dst=%u\r\n",
                  (unsigned)pending_ack.seq,
                  (unsigned)pending_ack.dst_id);
        return;
    }

    if (App_QueueRaw(pending_ack.raw, pending_ack.raw_len))
    {
        pending_ack.retries_left--;
        pending_ack.deadline_ms = now_ms + APP_ACK_TIMEOUT_MS;
        stat_retry++;
        Uart_Logf("RETRY seq=%u left=%u\r\n",
                  (unsigned)pending_ack.seq,
                  (unsigned)pending_ack.retries_left);
    }
    else
    {
        pending_ack.deadline_ms = now_ms + 120U;
    }
}

static void App_ProcessAckFrame(const app_frame_t *frame)
{
    if (frame == NULL)
    {
        return;
    }

    stat_ack_rx++;

    if ((pending_ack.active != 0U) &&
        (frame->src_id == pending_ack.dst_id) &&
        (frame->seq == pending_ack.seq))
    {
        pending_ack.active = 0U;
        Uart_Logf("ACK OK seq=%u from=%u\r\n",
                  (unsigned)frame->seq,
                  (unsigned)frame->src_id);
    }
}

static void App_ScheduleJoinAck(uint16_t nonce, uint16_t assigned_id, uint32_t now_ms)
{
    pending_join_ack.active = 1U;
    pending_join_ack.nonce = nonce;
    pending_join_ack.assigned_id = assigned_id;
    pending_join_ack.repeats_left = APP_JOIN_ACK_REPEAT;
    pending_join_ack.next_tx_ms = now_ms + APP_JOIN_ACK_DELAY_MS;
}

static void App_ProcessJoinAckBurst(uint32_t now_ms)
{
    app_frame_t reply;

    if (pending_join_ack.active == 0U)
    {
        return;
    }

    if ((int32_t)(now_ms - pending_join_ack.next_tx_ms) < 0)
    {
        return;
    }

    App_InitFrame(&reply, APP_MSG_JOIN_ACK, APP_COORDINATOR_ID, APP_BROADCAST_ID, next_seq++);
    reply.payload_len = 4U;
    reply.payload[0] = (uint8_t)(pending_join_ack.nonce & 0xFFU);
    reply.payload[1] = (uint8_t)((pending_join_ack.nonce >> 8) & 0xFFU);
    reply.payload[2] = (uint8_t)(pending_join_ack.assigned_id & 0xFFU);
    reply.payload[3] = (uint8_t)((pending_join_ack.assigned_id >> 8) & 0xFFU);

    if (!App_QueueFrame(&reply))
    {
        pending_join_ack.next_tx_ms = now_ms + 30U;
        return;
    }

    stat_join_ack++;
    pending_join_ack.repeats_left--;
    if (pending_join_ack.repeats_left == 0U)
    {
        pending_join_ack.active = 0U;
    }
    else
    {
        pending_join_ack.next_tx_ms = now_ms + APP_JOIN_ACK_GAP_MS;
    }
}

static void App_ScheduleAckBurst(uint16_t dst_id, uint16_t seq, uint32_t now_ms)
{
    pending_ack_burst.active = 1U;
    pending_ack_burst.dst_id = dst_id;
    pending_ack_burst.seq = seq;
    pending_ack_burst.repeats_left = APP_ACK_BURST_REPEAT;
    pending_ack_burst.next_tx_ms = now_ms + APP_ACK_BURST_DELAY_MS;
}

static void App_ProcessAckBurst(uint32_t now_ms)
{
    app_frame_t ack;

    if (pending_ack_burst.active == 0U)
    {
        return;
    }

    if ((int32_t)(now_ms - pending_ack_burst.next_tx_ms) < 0)
    {
        return;
    }

    App_InitFrame(&ack, APP_MSG_ACK, APP_COORDINATOR_ID, pending_ack_burst.dst_id, pending_ack_burst.seq);
    ack.payload_len = 0U;

    if (!App_QueueFrame(&ack))
    {
        pending_ack_burst.next_tx_ms = now_ms + 30U;
        return;
    }

    stat_ack_tx++;
    pending_ack_burst.repeats_left--;
    if (pending_ack_burst.repeats_left == 0U)
    {
        pending_ack_burst.active = 0U;
    }
    else
    {
        pending_ack_burst.next_tx_ms = now_ms + APP_ACK_BURST_GAP_MS;
    }
}

static void App_HandleJoinReq(const app_frame_t *frame)
{
    uint16_t nonce;
    uint16_t assigned_id;
    uint32_t now_ms;

    if ((frame == NULL) || (frame->payload_len < 2U))
    {
        return;
    }

    if ((frame->dst_id != APP_COORDINATOR_ID) && (frame->dst_id != APP_BROADCAST_ID))
    {
        return;
    }

    nonce = (uint16_t)frame->payload[0] | ((uint16_t)frame->payload[1] << 8);
    assigned_id = App_JoinFindOrAssign(nonce);
    if (assigned_id == APP_NODE_UNASSIGNED)
    {
        return;
    }

    stat_join_req++;
    Uart_LogTimedf("JOIN_REQ src=%u nonce=%u\r\n",
                   (unsigned)frame->src_id,
                   (unsigned)nonce);

    now_ms = HAL_GetTick();
    App_ScheduleJoinAck(nonce, assigned_id, now_ms);
    Uart_LogTimedf("JOIN_ACK scheduled nonce=%u -> id=%u x%u\r\n",
                   (unsigned)nonce,
                   (unsigned)assigned_id,
                   (unsigned)APP_JOIN_ACK_REPEAT);
}

static void App_HandleText(const app_frame_t *frame, int16_t rssi, int8_t snr)
{
    app_frame_t ack;
    uint32_t now_ms;

    if (frame == NULL)
    {
        return;
    }

    if ((frame->dst_id != APP_COORDINATOR_ID) && (frame->dst_id != APP_BROADCAST_ID))
    {
        return;
    }

    stat_text_rx++;
    {
        char text_buf[APP_MAX_PAYLOAD + 1U];
        uint8_t i;

        for (i = 0U; i < frame->payload_len; i++)
        {
            uint8_t c = frame->payload[i];
            if ((c < 32U) || (c > 126U))
            {
                c = '.';
            }
            text_buf[i] = (char)c;
        }
        text_buf[frame->payload_len] = '\0';

        Uart_LogTimedf("RX MSG src=%u dst=%u seq=%u RSSI=%d SNR=%d \"%s\"\r\n",
                       (unsigned)frame->src_id,
                       (unsigned)frame->dst_id,
                       (unsigned)frame->seq,
                       (int)rssi,
                       (int)snr,
                       text_buf);
    }

    if ((frame->dst_id != APP_BROADCAST_ID) && (frame->src_id != APP_NODE_UNASSIGNED))
    {
        now_ms = HAL_GetTick();
        App_InitFrame(&ack, APP_MSG_ACK, APP_COORDINATOR_ID, frame->src_id, frame->seq);
        if (App_QueueFrame(&ack))
        {
            stat_ack_tx++;
        }
        App_ScheduleAckBurst(frame->src_id, frame->seq, now_ms);
    }
}

static void App_HandleFrame(const app_frame_t *frame, int16_t rssi, int8_t snr)
{
    if (frame == NULL)
    {
        return;
    }

    switch (frame->type)
    {
    case APP_MSG_JOIN_REQ:
        App_HandleJoinReq(frame);
        break;

    case APP_MSG_TEXT:
        App_HandleText(frame, rssi, snr);
        break;

    case APP_MSG_ACK:
        App_ProcessAckFrame(frame);
        break;

    case APP_MSG_HEARTBEAT:
        /* Presence is tracked from src_id in App_HandleRxRaw. */
        break;

    default:
        break;
    }
}

static void App_HandleRxRaw(const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr)
{
    app_frame_t frame;
    uint32_t now_ms;

    stat_rx_total++;

    if (!App_ParseFrame(&frame, data, len))
    {
        stat_parse_err++;
        return;
    }

    if (frame.version != APP_PROTOCOL_VERSION)
    {
        stat_parse_err++;
        return;
    }

    now_ms = HAL_GetTick();
    /*
     * Evaluate offline timeouts before marking the current frame source as seen.
     * This guarantees proper disconnect/reconnect ordering even on edge timings.
     */
    App_CheckNodeDisconnects(now_ms);
    App_MarkNodeSeen(frame.src_id, now_ms);
    App_RelayFrame(&frame);
    App_HandleFrame(&frame, rssi, snr);
}

static void App_SendText(uint16_t dst_id, const char *text, uint32_t now_ms)
{
    app_frame_t frame;
    uint8_t raw[APP_MAX_FRAME_LEN];
    uint8_t raw_len;
    uint8_t text_len;

    if (text == NULL)
    {
        return;
    }

    while ((*text == ' ') || (*text == '\t'))
    {
        text++;
    }

    if (*text == '\0')
    {
        return;
    }

    if ((dst_id != APP_BROADCAST_ID) && (pending_ack.active != 0U))
    {
        Uart_Log("ERR: wait ACK before next direct message\r\n");
        return;
    }

    if (tx_queue_count != 0U)
    {
        Uart_Log("ERR: tx queue busy, retry in a moment\r\n");
        return;
    }

    text_len = (uint8_t)strlen(text);
    if (text_len > APP_MAX_PAYLOAD)
    {
        text_len = APP_MAX_PAYLOAD;
        Uart_Log("INFO: text truncated to 64 bytes\r\n");
    }

    App_InitFrame(&frame, APP_MSG_TEXT, APP_COORDINATOR_ID, dst_id, next_seq++);
    frame.payload_len = text_len;
    memcpy(frame.payload, text, text_len);

    raw_len = App_SerializeFrame(&frame, raw, (uint8_t)sizeof(raw));
    if (raw_len == 0U)
    {
        Uart_Log("TX FAIL: serialize\r\n");
        return;
    }

    if (!App_QueueRaw(raw, raw_len))
    {
        Uart_Log("TX FAIL: queue full\r\n");
        return;
    }

    if (dst_id != APP_BROADCAST_ID)
    {
        App_StartPendingAck(dst_id, frame.seq, raw, raw_len, now_ms);
    }

    Uart_LogTimedf("TX MSG dst=%u seq=%u\r\n",
                   (unsigned)dst_id,
                   (unsigned)frame.seq);
}

static void App_PrintHelp(void)
{
    Uart_Log("Commands:\r\n");
    Uart_Log("  id                  -> show coordinator id\r\n");
    Uart_Log("  nodes               -> list joined nodes\r\n");
    Uart_Log("  status              -> print radio/protocol counters\r\n");
    Uart_Log("  dst <id>            -> set default destination\r\n");
    Uart_Log("  send <id> <text>    -> send reliable text (ACK/retry)\r\n");
    Uart_Log("  broadcast <text>    -> send broadcast text\r\n");
    Uart_Log("  help                -> show this help\r\n");
    Uart_Log("  <text>              -> send to default destination\r\n");
    Uart_Log("Note: lines auto-submit after a short pause.\r\n");
}

static void App_HandleUartLine(char *line, uint32_t now_ms)
{
    char *cursor;
    uint16_t id;

    if (line == NULL)
    {
        return;
    }

    cursor = SkipSpaces(line);
    if (*cursor == '\0')
    {
        return;
    }

    if ((strcmp(cursor, "help") == 0) || (strcmp(cursor, "?") == 0))
    {
        App_PrintHelp();
        return;
    }

    if (strcmp(cursor, "id") == 0)
    {
        Uart_Logf("Coordinator id=%u defaultDst=%u\r\n",
                  (unsigned)APP_COORDINATOR_ID,
                  (unsigned)default_dst_id);
        return;
    }

    if (strcmp(cursor, "nodes") == 0)
    {
        App_PrintNodes();
        return;
    }

    if (strcmp(cursor, "status") == 0)
    {
        App_PrintStatus();
        return;
    }

    if (strncmp(cursor, "dst", 3U) == 0)
    {
        cursor += 3;
        if (!ParseU16(&cursor, &id))
        {
            Uart_Log("Usage: dst <id>\r\n");
            return;
        }
        cursor = SkipSpaces(cursor);
        if (*cursor != '\0')
        {
            Uart_Log("Usage: dst <id>\r\n");
            return;
        }

        default_dst_id = id;
        Uart_Logf("Default dst=%u\r\n", (unsigned)default_dst_id);
        return;
    }

    if (strncmp(cursor, "send", 4U) == 0)
    {
        char *msg;
        cursor += 4;
        if (!ParseU16(&cursor, &id))
        {
            Uart_Log("Usage: send <id> <text>\r\n");
            return;
        }

        msg = SkipSpaces(cursor);
        if (*msg == '\0')
        {
            Uart_Log("Usage: send <id> <text>\r\n");
            return;
        }

        App_SendText(id, msg, now_ms);
        return;
    }

    if (strncmp(cursor, "broadcast", 9U) == 0)
    {
        char *msg = SkipSpaces(cursor + 9U);
        if (*msg == '\0')
        {
            Uart_Log("Usage: broadcast <text>\r\n");
            return;
        }

        App_SendText(APP_BROADCAST_ID, msg, now_ms);
        return;
    }

    App_SendText(default_dst_id, cursor, now_ms);
}

static void App_PollUart(uint32_t now_ms)
{
    char c;
    uint8_t src;

    while (Uart_TryReadChar(&c))
    {
        src = uart_last_read_src;
        uart_last_rx_char_ms = now_ms;

        if (uart_rx_seen == 0U)
        {
            uart_rx_seen = 1U;
            Uart_Log("\r\nUART RX OK\r\n> ");
        }

        if ((c == '\r') || (c == '\n'))
        {
            App_SubmitUartLine(now_ms);
            continue;
        }

        if ((c == '\b') || ((uint8_t)c == 0x7FU))
        {
            /*
             * Ignore backspace/delete control bytes.
             * This prevents 1-char truncation when these bytes are injected
             * by a secondary UART bridge or terminal.
             */
            continue;
        }

        if (((uint8_t)c >= 32U) && ((uint8_t)c <= 126U))
        {
            /*
             * Keep each submitted line bound to a single UART source.
             * This avoids mixed lines when both USARTs are connected.
             */
            if ((uart_line_src != 0U) && (src != 0U) && (src != uart_line_src))
            {
                continue;
            }
            if ((uart_line_src == 0U) && (src != 0U))
            {
                uart_line_src = src;
            }
            if (uart_line_len < (APP_UART_LINE_MAX - 1U))
            {
                uart_line[uart_line_len++] = c;
#if (APP_UART_ECHO != 0U)
                Uart_TransmitAll((const uint8_t *)&c, 1U);
#endif
            }
        }
    }

    if ((uart_line_len > 0U) &&
        ((now_ms - uart_last_rx_char_ms) >= APP_UART_AUTOSUBMIT_MS))
    {
        App_SubmitUartLine(now_ms);
    }
}

static void App_SubmitUartLine(uint32_t now_ms)
{
    if (uart_line_len == 0U)
    {
        return;
    }

    uart_line[uart_line_len] = '\0';
#if (APP_UART_ECHO != 0U)
    Uart_Log("\r\n");
#endif
    App_HandleUartLine(uart_line, now_ms);
    uart_line_len = 0U;
    uart_line_src = 0U;
    Uart_Log("> ");
}

static void Radio_Init(void)
{
    radio_events.TxDone = OnTxDone;
    radio_events.RxDone = OnRxDone;
    radio_events.TxTimeout = OnTxTimeout;
    radio_events.RxTimeout = OnRxTimeout;
    radio_events.RxError = OnRxError;

    Radio.Init(&radio_events);
    Radio.SetChannel(RF_FREQUENCY_HZ);

    Radio.SetTxConfig(MODEM_LORA,
                      TX_OUTPUT_POWER_DBM,
                      0,
                      LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH,
                      false,
                      true,
                      0,
                      0,
                      false,
                      TX_TIMEOUT_MS);

    Radio.SetRxConfig(MODEM_LORA,
                      LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE,
                      0,
                      LORA_PREAMBLE_LENGTH,
                      LORA_SYMBOL_TIMEOUT,
                      false,
                      0,
                      true,
                      0,
                      0,
                      false,
                      true);
}

static void Radio_Reapply_Config(void)
{
    Radio.SetPublicNetwork(false);
    Radio.SetChannel(RF_FREQUENCY_HZ);

    Radio.SetTxConfig(MODEM_LORA,
                      TX_OUTPUT_POWER_DBM,
                      0,
                      LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH,
                      false,
                      true,
                      0,
                      0,
                      false,
                      TX_TIMEOUT_MS);

    Radio.SetRxConfig(MODEM_LORA,
                      LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE,
                      0,
                      LORA_PREAMBLE_LENGTH,
                      LORA_SYMBOL_TIMEOUT,
                      false,
                      0,
                      true,
                      0,
                      0,
                      false,
                      true);
}

static void OnTxDone(void)
{
    radio_tx_done = 1U;
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (size > sizeof(rx_buffer))
    {
        size = sizeof(rx_buffer);
    }

    memcpy(rx_buffer, payload, size);
    rx_size = size;
    rx_last_rssi = rssi;
    rx_last_snr = snr;
    radio_rx_done = 1U;
}

static void OnTxTimeout(void)
{
    radio_tx_timeout = 1U;
}

static void OnRxTimeout(void)
{
    radio_rx_timeout = 1U;
}

static void OnRxError(void)
{
    radio_rx_error = 1U;
}

static void Uart_Log(const char *msg)
{
    if (msg == NULL)
    {
        return;
    }
    Uart_TransmitAll((const uint8_t *)msg, (uint16_t)strlen(msg));
}

static void Uart_Logf(const char *fmt, ...)
{
    char buf[196];
    va_list ap;

    if (fmt == NULL)
    {
        return;
    }

    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Uart_Log(buf);
}

static void Uart_LogTimedf(const char *fmt, ...)
{
    char msg[196];
    char ts[20];
    char line[236];
    uint32_t now;
    uint32_t total_sec;
    uint32_t h;
    uint32_t m;
    uint32_t s;
    uint32_t ms;
    va_list ap;

    if (fmt == NULL)
    {
        return;
    }

    now = HAL_GetTick();
    total_sec = now / 1000U;
    h = (total_sec / 3600U) % 24U;
    m = (total_sec % 3600U) / 60U;
    s = total_sec % 60U;
    ms = now % 1000U;
    (void)snprintf(ts, sizeof(ts), "%02lu:%02lu:%02lu.%03lu",
                   (unsigned long)h,
                   (unsigned long)m,
                   (unsigned long)s,
                   (unsigned long)ms);

    va_start(ap, fmt);
    (void)vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    (void)snprintf(line, sizeof(line), "[%s] %s", ts, msg);
    Uart_Log(line);
}

static void Uart_RingReset(app_uart_ring_t *ring)
{
    if (ring == NULL)
    {
        return;
    }

    ring->head = 0U;
    ring->tail = 0U;
    ring->overflow_count = 0U;
}

static void Uart_RingPush(app_uart_ring_t *ring, uint8_t value)
{
    uint16_t head;
    uint16_t next;
    uint16_t tail;

    if (ring == NULL)
    {
        return;
    }

    head = ring->head;
    tail = ring->tail;
    next = (uint16_t)(head + 1U);
    if (next >= APP_UART_RX_RING_SIZE)
    {
        next = 0U;
    }

    if (next == tail)
    {
        /* Drop oldest byte to keep newest command input. */
        tail = (uint16_t)(tail + 1U);
        if (tail >= APP_UART_RX_RING_SIZE)
        {
            tail = 0U;
        }
        ring->tail = tail;
        ring->overflow_count++;
    }

    ring->data[head] = value;
    ring->head = next;
}

static uint8_t Uart_RingPop(app_uart_ring_t *ring, char *out_char)
{
    uint16_t tail;
    uint16_t head;

    if ((ring == NULL) || (out_char == NULL))
    {
        return 0U;
    }

    tail = ring->tail;
    head = ring->head;
    if (tail == head)
    {
        return 0U;
    }

    *out_char = (char)ring->data[tail];
    tail = (uint16_t)(tail + 1U);
    if (tail >= APP_UART_RX_RING_SIZE)
    {
        tail = 0U;
    }
    ring->tail = tail;
    return 1U;
}

static void Uart_IrqDrain(UART_HandleTypeDef *huart, app_uart_ring_t *ring)
{
    USART_TypeDef *uartx;
    uint32_t isr;

    if ((huart == NULL) || (ring == NULL) || (huart->Instance == NULL))
    {
        return;
    }

    uartx = huart->Instance;
    isr = uartx->ISR;

    if ((isr & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)) != 0U)
    {
        uartx->ICR = USART_ICR_ORECF | USART_ICR_NECF | USART_ICR_FECF | USART_ICR_PECF;
    }

#if defined(USART_ISR_RXNE_RXFNE)
    while ((uartx->ISR & USART_ISR_RXNE_RXFNE) != 0U)
#else
    while ((uartx->ISR & USART_ISR_RXNE) != 0U)
#endif
    {
        Uart_RingPush(ring, (uint8_t)(uartx->RDR & 0xFFU));
    }
}

void USART1_IRQHandler(void)
{
    if (uart1_ready != 0U)
    {
        Uart_IrqDrain(&huart1, &uart1_rx_ring);
    }
}

void USART2_IRQHandler(void)
{
    if (uart2_ready != 0U)
    {
        Uart_IrqDrain(&huart2, &uart2_rx_ring);
    }
}

static void Uart_TransmitAll(const uint8_t *buf, uint16_t len)
{
    if ((buf == NULL) || (len == 0U))
    {
        return;
    }

#if (APP_CONSOLE_UART == 2U)
    if (uart2_ready != 0U)
    {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 100U);
    }
#if (APP_UART_MIRROR_BOTH != 0U)
    if (uart1_ready != 0U)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100U);
    }
#else
    /* Fallback if preferred console UART is not available */
    if ((uart2_ready == 0U) && (uart1_ready != 0U))
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100U);
    }
#endif
#else
    if (uart1_ready != 0U)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100U);
    }
#if (APP_UART_MIRROR_BOTH != 0U)
    if (uart2_ready != 0U)
    {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 100U);
    }
#else
    /* Fallback if preferred console UART is not available */
    if ((uart1_ready == 0U) && (uart2_ready != 0U))
    {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 100U);
    }
#endif
#endif
}

static uint8_t Uart_TryReadChar(char *out_char)
{
    if (out_char == NULL)
    {
        return 0U;
    }

    uart_last_read_src = 0U;

#if (APP_CONSOLE_UART == 2U)
    if (Uart_RingPop(&uart2_rx_ring, out_char) != 0U)
    {
        uart_last_read_src = 2U;
        return 1U;
    }
    if (Uart_RingPop(&uart1_rx_ring, out_char) != 0U)
    {
        uart_last_read_src = 1U;
        return 1U;
    }
#else
    if (Uart_RingPop(&uart1_rx_ring, out_char) != 0U)
    {
        uart_last_read_src = 1U;
        return 1U;
    }
    if (Uart_RingPop(&uart2_rx_ring, out_char) != 0U)
    {
        uart_last_read_src = 2U;
        return 1U;
    }
#endif

    /*
     * Safety fallback: direct register polling in case IRQ/RXNE config is not active yet.
     */
#if (APP_CONSOLE_UART == 2U)
    if (Uart_TryReadFrom(&huart2, out_char) != 0U)
    {
        uart_last_read_src = 2U;
        return 1U;
    }
    if (Uart_TryReadFrom(&huart1, out_char) != 0U)
    {
        uart_last_read_src = 1U;
        return 1U;
    }
#else
    if (Uart_TryReadFrom(&huart1, out_char) != 0U)
    {
        uart_last_read_src = 1U;
        return 1U;
    }
    if (Uart_TryReadFrom(&huart2, out_char) != 0U)
    {
        uart_last_read_src = 2U;
        return 1U;
    }
#endif

    return 0U;
}

static uint8_t Uart_TryReadFrom(UART_HandleTypeDef *huart, char *out_char)
{
    USART_TypeDef *uartx;
    uint32_t isr;

    if ((huart == NULL) || (out_char == NULL) || (huart->Instance == NULL))
    {
        return 0U;
    }

    if ((huart->Instance == USART1) && (uart1_ready == 0U))
    {
        return 0U;
    }
    if ((huart->Instance == USART2) && (uart2_ready == 0U))
    {
        return 0U;
    }

    uartx = huart->Instance;
    isr = uartx->ISR;

    /* Clear sticky RX error flags so reception keeps working after glitches. */
    if ((isr & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)) != 0U)
    {
        uartx->ICR = USART_ICR_ORECF | USART_ICR_NECF | USART_ICR_FECF | USART_ICR_PECF;
    }

#if defined(USART_ISR_RXNE_RXFNE)
    if ((uartx->ISR & USART_ISR_RXNE_RXFNE) != 0U)
#else
    if ((uartx->ISR & USART_ISR_RXNE) != 0U)
#endif
    {
        *out_char = (char)(uint8_t)(uartx->RDR & 0xFFU);
        return 1U;
    }

    return 0U;
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
}

static void MX_USART_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    Uart_RingReset(&uart1_rx_ring);
    Uart_RingReset(&uart2_rx_ring);

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) == HAL_OK)
    {
        __HAL_UART_ENABLE_IT(&huart2, UART_IT_ERR);
#if defined(USART_CR1_RXNEIE_RXFNEIE)
        SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE_RXFNEIE);
#else
        __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
#endif
        HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);
        uart2_ready = 1U;
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) == HAL_OK)
    {
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_ERR);
#if defined(USART_CR1_RXNEIE_RXFNEIE)
        SET_BIT(huart1.Instance->CR1, USART_CR1_RXNEIE_RXFNEIE);
#else
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
#endif
        HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
        uart1_ready = 1U;
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, LL_FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
