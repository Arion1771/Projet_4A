#include "wyresv2_node.h"

#include <string.h>

#define WYRESV2_PROTO_VERSION        1U
#define WYRESV2_SEQ_START            1U
#define WYRESV2_UNKNOWN_NODE_ID      0xFFFFU
#define WYRESV2_BOOTING_MS           600U
#define WYRESV2_DISCOVERY_MS         3000U
#define WYRESV2_HEARTBEAT_MS         2000U
#define WYRESV2_DEFAULT_TTL          8U

static void wyresv2_apply_state_led(wyresv2_state_t state)
{
    switch (state) {
    case NODE_OFF:
        platform_led_set(LED_OFF);
        break;
    case NODE_BOOTING:
    case NODE_DISCOVERING:
        platform_led_set(LED_NEUTRAL);
        break;
    case NODE_INSERTED:
        platform_led_set(LED_INSERTED);
        break;
    case NODE_ALERT_ACTIVE:
    default:
        platform_led_set(LED_ALERT);
        break;
    }
}

static void wyresv2_set_state(wyresv2_ctx_t *ctx, wyresv2_state_t state)
{
    if (ctx == NULL) {
        return;
    }

    ctx->state = state;
    wyresv2_apply_state_led(state);
}

static uint8_t wyresv2_serialize(const wyresv2_frame_t *frame, uint8_t *out, uint8_t out_max)
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

static bool wyresv2_send_frame(wyresv2_ctx_t *ctx, uint8_t type, uint16_t dst_id, const uint8_t *payload, uint8_t len)
{
    wyresv2_frame_t frame;
    uint8_t raw[11U + WYRESV2_MAX_PAYLOAD];
    uint8_t raw_len;

    if ((ctx == NULL) || ((payload == NULL) && (len > 0U)) || (len > WYRESV2_MAX_PAYLOAD)) {
        return false;
    }

    memset(&frame, 0, sizeof(frame));
    frame.version = WYRESV2_PROTO_VERSION;
    frame.type = type;
    frame.src_id = ctx->node_id;
    frame.dst_id = dst_id;
    frame.seq = ctx->next_seq++;
    frame.ttl = WYRESV2_DEFAULT_TTL;
    frame.flags = 0U;
    frame.payload_len = len;
    if (len > 0U) {
        memcpy(frame.payload, payload, len);
    }

    raw_len = wyresv2_serialize(&frame, raw, (uint8_t)sizeof(raw));
    if (raw_len == 0U) {
        return false;
    }

    return platform_radio_send(LINK_SUCCESSOR, raw, raw_len);
}

void wyresv2_init(wyresv2_ctx_t *ctx, uint16_t node_id)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->node_id = node_id;
    ctx->predecessor_id = WYRESV2_UNKNOWN_NODE_ID;
    ctx->successor_id = WYRESV2_UNKNOWN_NODE_ID;
    ctx->next_seq = WYRESV2_SEQ_START;
    ctx->state = NODE_OFF;
    ctx->inserted = false;
    wyresv2_apply_state_led(NODE_OFF);
}

void wyresv2_power_on(wyresv2_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->inserted = false;
    ctx->last_heartbeat_ms = platform_millis();
    wyresv2_set_state(ctx, NODE_BOOTING);
}

void wyresv2_tick(wyresv2_ctx_t *ctx, uint32_t now_ms)
{
    static const uint8_t hb_payload[] = {'H', 'B'};
    uint32_t elapsed;

    if (ctx == NULL) {
        return;
    }

    elapsed = now_ms - ctx->last_heartbeat_ms;

    switch (ctx->state) {
    case NODE_OFF:
        break;

    case NODE_BOOTING:
        if (elapsed >= WYRESV2_BOOTING_MS) {
            ctx->last_heartbeat_ms = now_ms;
            wyresv2_set_state(ctx, NODE_DISCOVERING);
        }
        break;

    case NODE_DISCOVERING:
        if (elapsed >= WYRESV2_DISCOVERY_MS) {
            ctx->inserted = true;
            ctx->last_heartbeat_ms = now_ms;
            wyresv2_set_state(ctx, NODE_INSERTED);
        }
        break;

    case NODE_INSERTED:
        if (elapsed >= WYRESV2_HEARTBEAT_MS) {
            (void)wyresv2_send_frame(ctx, MSG_HEARTBEAT, WYRESV2_UNKNOWN_NODE_ID, hb_payload, (uint8_t)sizeof(hb_payload));
            ctx->last_heartbeat_ms = now_ms;
        }
        break;

    case NODE_ALERT_ACTIVE:
    default:
        break;
    }
}

void wyresv2_on_frame(wyresv2_ctx_t *ctx, link_direction_t from, const wyresv2_frame_t *frame, int16_t rssi, int8_t snr)
{
    if ((ctx == NULL) || (frame == NULL)) {
        return;
    }

    if (from == LINK_PREDECESSOR) {
        ctx->last_rssi_pred = rssi;
        ctx->last_snr_pred = snr;
        ctx->predecessor_id = frame->src_id;
    } else {
        ctx->last_rssi_succ = rssi;
        ctx->last_snr_succ = snr;
        ctx->successor_id = frame->src_id;
    }

    switch (frame->type) {
    case MSG_ALERT:
        wyresv2_set_state(ctx, NODE_ALERT_ACTIVE);
        break;

    case MSG_JOIN_ACK:
    case MSG_HEARTBEAT:
        if (ctx->state != NODE_ALERT_ACTIVE) {
            ctx->inserted = true;
            ctx->last_heartbeat_ms = platform_millis();
            wyresv2_set_state(ctx, NODE_INSERTED);
        }
        break;

    default:
        break;
    }
}

bool wyresv2_send_text(wyresv2_ctx_t *ctx, uint16_t dst_id, const uint8_t *payload, uint8_t len)
{
    return wyresv2_send_frame(ctx, MSG_TEXT, dst_id, payload, len);
}

bool wyresv2_send_alert(wyresv2_ctx_t *ctx, uint16_t dst_id, const uint8_t *payload, uint8_t len)
{
    bool ok;

    if (ctx == NULL) {
        return false;
    }

    wyresv2_set_state(ctx, NODE_ALERT_ACTIVE);
    ok = wyresv2_send_frame(ctx, MSG_ALERT, dst_id, payload, len);
    return ok;
}
