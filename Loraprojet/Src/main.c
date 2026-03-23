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
#define APP_JOIN_TABLE_SIZE    16U
#define APP_JOIN_FIRST_ID      2U
#define APP_JOIN_LAST_ID       250U
#define APP_ACK_TIMEOUT_MS     700U
#define APP_ACK_MAX_RETRIES    3U

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
static app_join_entry_t join_table[APP_JOIN_TABLE_SIZE];
static uint16_t next_assigned_id = APP_JOIN_FIRST_ID;
static uint16_t next_seq = 1U;
static uint16_t default_dst_id = APP_BROADCAST_ID;

static char uart_line[APP_UART_LINE_MAX];
static uint8_t uart_line_len = 0U;

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
static void Uart_LogText(const uint8_t *data, uint16_t size);
static void Uart_Logf(const char *fmt, ...);
static void Uart_LogTimedf(const char *fmt, ...);
static uint8_t Uart_TryReadChar(char *out_char);

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
static void App_PrintNodes(void);

static void App_StartPendingAck(uint16_t dst_id, uint16_t seq, const uint8_t *raw, uint8_t raw_len, uint32_t now_ms);
static void App_ProcessPendingAck(uint32_t now_ms);
static void App_ProcessAckFrame(const app_frame_t *frame);

static void App_HandleJoinReq(const app_frame_t *frame);
static void App_HandleText(const app_frame_t *frame, int16_t rssi, int8_t snr);
static void App_HandleFrame(const app_frame_t *frame, int16_t rssi, int8_t snr);
static void App_HandleRxRaw(const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr);

static void App_SendText(uint16_t dst_id, const char *text, uint32_t now_ms);
static void App_PrintHelp(void);
static void App_HandleUartLine(char *line, uint32_t now_ms);
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
    memset(join_table, 0, sizeof(join_table));

    Uart_Log("BOOT LORAPROJET\r\n");
    Uart_Log("MODE: CARD1 COORDINATOR (LORAE5)\r\n");
    Uart_Log("IRQ MODE: ISR_DIRECT\r\n");
    Uart_Log("BUILD: " __DATE__ " " __TIME__ "\r\n");
    Uart_Log("RFSW PROFILE ID: " STR(RBI_RF_SW_PROFILE) "\r\n");
#if (RBI_RF_SW_PROFILE == RBI_RF_SW_PROFILE_WYRES_REVC)
    Uart_Log("RFSW PROFILE: WYRES_REVC\r\n");
#else
    Uart_Log("RFSW PROFILE: LORAE5\r\n");
#endif
    Uart_Log("COORDINATOR NODE ID: 1\r\n");

    Radio_Init();
    Radio.SetPublicNetwork(false);
    Radio.Rx(0);

    App_PrintHelp();
    Uart_Log("> ");

    while (1)
    {
        now_ms = HAL_GetTick();

#if (APP_RADIO_IRQ_IN_ISR == 0U)
        Radio.IrqProcess();
#endif
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

    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++)
    {
        if (join_table[i].used)
        {
            if (join_table[i].nonce == nonce)
            {
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
    return id;
}

static void App_PrintNodes(void)
{
    uint8_t i;
    uint8_t found = 0U;

    Uart_Log("Known nodes: ");
    for (i = 0U; i < APP_JOIN_TABLE_SIZE; i++)
    {
        if (join_table[i].used)
        {
            found = 1U;
            Uart_Logf("[id=%u nonce=%u] ",
                      (unsigned)join_table[i].node_id,
                      (unsigned)join_table[i].nonce);
        }
    }
    if (found == 0U)
    {
        Uart_Log("none");
    }
    Uart_Log("\r\n");
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

static void App_HandleJoinReq(const app_frame_t *frame)
{
    app_frame_t reply;
    uint16_t nonce;
    uint16_t assigned_id;

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
    App_InitFrame(&reply, APP_MSG_JOIN_ACK, APP_COORDINATOR_ID, APP_BROADCAST_ID, next_seq++);
    reply.payload_len = 4U;
    reply.payload[0] = (uint8_t)(nonce & 0xFFU);
    reply.payload[1] = (uint8_t)((nonce >> 8) & 0xFFU);
    reply.payload[2] = (uint8_t)(assigned_id & 0xFFU);
    reply.payload[3] = (uint8_t)((assigned_id >> 8) & 0xFFU);

    if (App_QueueFrame(&reply))
    {
        stat_join_ack++;
        Uart_Logf("JOIN_ACK nonce=%u -> id=%u\r\n",
                  (unsigned)nonce,
                  (unsigned)assigned_id);
    }
}

static void App_HandleText(const app_frame_t *frame, int16_t rssi, int8_t snr)
{
    app_frame_t ack;

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
        App_InitFrame(&ack, APP_MSG_ACK, APP_COORDINATOR_ID, frame->src_id, frame->seq);
        if (App_QueueFrame(&ack))
        {
            stat_ack_tx++;
        }
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

    default:
        break;
    }
}

static void App_HandleRxRaw(const uint8_t *data, uint16_t len, int16_t rssi, int8_t snr)
{
    app_frame_t frame;

    stat_rx_total++;

    if (!App_ParseFrame(&frame, data, len))
    {
        stat_parse_err++;
        Uart_Log("RX RAW: ");
        Uart_LogText(data, len);
        Uart_Log("\r\n");
        return;
    }

    if (frame.version != APP_PROTOCOL_VERSION)
    {
        stat_parse_err++;
        return;
    }

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
    Uart_Log("  dst <id>            -> set default destination\r\n");
    Uart_Log("  send <id> <text>    -> send reliable text (ACK/retry)\r\n");
    Uart_Log("  broadcast <text>    -> send broadcast text\r\n");
    Uart_Log("  help                -> show this help\r\n");
    Uart_Log("  <text>              -> send to default destination\r\n");
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

    while (Uart_TryReadChar(&c))
    {
        if ((c == '\r') || (c == '\n'))
        {
            if (uart_line_len > 0U)
            {
                uart_line[uart_line_len] = '\0';
                Uart_Log("\r\n");
                App_HandleUartLine(uart_line, now_ms);
                uart_line_len = 0U;
                Uart_Log("> ");
            }
            continue;
        }

        if ((c == '\b') || ((uint8_t)c == 0x7FU))
        {
            if (uart_line_len > 0U)
            {
                uart_line_len--;
                Uart_Log("\b \b");
            }
            continue;
        }

        if (((uint8_t)c >= 32U) && ((uint8_t)c <= 126U))
        {
            if (uart_line_len < (APP_UART_LINE_MAX - 1U))
            {
                uart_line[uart_line_len++] = c;
                Uart_TransmitAll((const uint8_t *)&c, 1U);
            }
        }
    }
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

static void Uart_LogText(const uint8_t *data, uint16_t size)
{
    uint16_t i;
    uint8_t c;

    if (data == NULL)
    {
        return;
    }

    for (i = 0U; i < size; i++)
    {
        c = data[i];
        if ((c < 32U) || (c > 126U))
        {
            c = '.';
        }
        Uart_TransmitAll(&c, 1U);
    }
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

static void Uart_TransmitAll(const uint8_t *buf, uint16_t len)
{
    if ((buf == NULL) || (len == 0U))
    {
        return;
    }

    if (uart1_ready != 0U)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100U);
    }

    if (uart2_ready != 0U)
    {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 100U);
    }
}

static uint8_t Uart_TryReadChar(char *out_char)
{
    HAL_StatusTypeDef st;
    uint8_t c;

    if (out_char == NULL)
    {
        return 0U;
    }

    if (uart1_ready != 0U)
    {
        st = HAL_UART_Receive(&huart1, &c, 1U, 0U);
        if (st == HAL_OK)
        {
            *out_char = (char)c;
            return 1U;
        }
    }

    if (uart2_ready != 0U)
    {
        st = HAL_UART_Receive(&huart2, &c, 1U, 0U);
        if (st == HAL_OK)
        {
            *out_char = (char)c;
            return 1U;
        }
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
