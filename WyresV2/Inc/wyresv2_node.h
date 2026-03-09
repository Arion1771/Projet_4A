#ifndef WYRESV2_NODE_H
#define WYRESV2_NODE_H

#include "platform_port.h"

#include <stdbool.h>
#include <stdint.h>

#define WYRESV2_MAX_PAYLOAD 64U
#define WYRESV2_MAX_PENDING 8U

typedef enum {
    NODE_OFF = 0,
    NODE_BOOTING,
    NODE_DISCOVERING,
    NODE_INSERTED,
    NODE_ALERT_ACTIVE
} wyresv2_state_t;

typedef enum {
    MSG_TEXT = 1,
    MSG_ALERT = 2,
    MSG_SUPERVISION = 3,
    MSG_ACK = 4,
    MSG_JOIN_REQ = 5,
    MSG_JOIN_ACK = 6,
    MSG_HEARTBEAT = 7
} wyresv2_msg_type_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t src_id;
    uint16_t dst_id;
    uint16_t seq;
    uint8_t ttl;
    uint8_t flags;
    uint8_t payload_len;
    uint8_t payload[WYRESV2_MAX_PAYLOAD];
} wyresv2_frame_t;

typedef struct {
    uint16_t node_id;
    uint16_t predecessor_id;
    uint16_t successor_id;
    wyresv2_state_t state;
    bool inserted;
    uint16_t next_seq;
    uint32_t last_heartbeat_ms;
    int16_t last_rssi_pred;
    int16_t last_rssi_succ;
    int8_t last_snr_pred;
    int8_t last_snr_succ;
} wyresv2_ctx_t;

void wyresv2_init(wyresv2_ctx_t *ctx, uint16_t node_id);
void wyresv2_power_on(wyresv2_ctx_t *ctx);
void wyresv2_tick(wyresv2_ctx_t *ctx, uint32_t now_ms);

/* Call on each incoming LoRa frame */
void wyresv2_on_frame(wyresv2_ctx_t *ctx, link_direction_t from, const wyresv2_frame_t *frame, int16_t rssi, int8_t snr);

/* Local application requests */
bool wyresv2_send_text(wyresv2_ctx_t *ctx, uint16_t dst_id, const uint8_t *payload, uint8_t len);
bool wyresv2_send_alert(wyresv2_ctx_t *ctx, uint16_t dst_id, const uint8_t *payload, uint8_t len);

#endif
