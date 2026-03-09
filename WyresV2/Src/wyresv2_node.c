#include "wyresv2_node.h"

#include <string.h>

enum {
    FRAME_VERSION = 1,
    FRAME_FLAG_ACK_REQUIRED = 0x01,
    RETRANSMIT_TIMEOUT_MS = 600,
    RETRANSMIT_MAX_ATTEMPTS = 3,
    HEARTBEAT_PERIOD_MS = 1500,
    HEARTBEAT_TIMEOUT_MS = 5000,
    DEFAULT_TTL = 16
};

typedef struct {
    bool used;
    wyresv2_frame_t frame;
    link_direction_t dir;
    uint32_t next_retry_ms;
    uint8_t retries;
    bool high_priority;
} pending_entry_t;

static pending_entry_t s_pending[WYRESV2_MAX_PENDING];

static link_direction_t opposite_dir(link_direction_t d)
{
    return (d == LINK_PREDECESSOR) ? LINK_SUCCESSOR : LINK_PREDECESSOR;
}

static bool send_frame(link_direction_t dir, const wyresv2_frame_t *f)
{
    return platform_radio_send(dir, (const uint8_t *)f, (uint16_t)(sizeof(*f) - WYRESV2_MAX_PAYLOAD + f->payload_len));
}

static void set_led_for_state(wyresv2_state_t state)
{
    if (state == NODE_OFF) {
        platform_led_set(LED_OFF);
    } else if (state == NODE_INSERTED) {
        platform_led_set(LED_INSERTED);
    } else if (state == NODE_ALERT_ACTIVE) {
        platform_led_set(LED_ALERT);
    } else {
        platform_led_set(LED_NEUTRAL);
    }
}

static bool queue_pending(const wyresv2_frame_t *frame, link_direction_t dir, uint32_t now_ms, bool high_priority)
{
    uint32_t i;
    for (i = 0; i < WYRESV2_MAX_PENDING; ++i) {
        if (!s_pending[i].used) {
            s_pending[i].used = true;
            s_pending[i].frame = *frame;
            s_pending[i].dir = dir;
            s_pending[i].next_retry_ms = now_ms + RETRANSMIT_TIMEOUT_MS;
            s_pending[i].retries = 0;
            s_pending[i].high_priority = high_priority;
            return true;
        }
    }
    return false;
}

static void ack_pending(uint16_t seq)
{
    uint32_t i;
    for (i = 0; i < WYRESV2_MAX_PENDING; ++i) {
        if (s_pending[i].used && s_pending[i].frame.seq == seq) {
            s_pending[i].used = false;
            return;
        }
    }
}

static bool send_ack(const wyresv2_ctx_t *ctx, link_direction_t to, uint16_t seq)
{
    wyresv2_frame_t ack;
    memset(&ack, 0, sizeof(ack));

    ack.version = FRAME_VERSION;
    ack.type = MSG_ACK;
    ack.src_id = ctx->node_id;
    ack.dst_id = 0xFFFF;
    ack.seq = seq;
    ack.ttl = 1;
    ack.flags = 0;
    ack.payload_len = 0;

    return send_frame(to, &ack);
}

static bool forward_with_ack(wyresv2_ctx_t *ctx, link_direction_t dir, wyresv2_frame_t *frame, bool high_priority)
{
    uint32_t now = platform_millis();

    if (frame->ttl == 0) {
        return false;
    }

    frame->ttl -= 1;

    if (!send_frame(dir, frame)) {
        return false;
    }

    if ((frame->flags & FRAME_FLAG_ACK_REQUIRED) != 0U) {
        if (!queue_pending(frame, dir, now, high_priority)) {
            return false;
        }
    }

    return true;
}

static void discover_and_join(wyresv2_ctx_t *ctx)
{
    wyresv2_frame_t req;

    memset(&req, 0, sizeof(req));
    req.version = FRAME_VERSION;
    req.type = MSG_JOIN_REQ;
    req.src_id = ctx->node_id;
    req.dst_id = 0xFFFF;
    req.seq = ctx->next_seq++;
    req.ttl = 1;
    req.flags = FRAME_FLAG_ACK_REQUIRED;
    req.payload_len = 0;

    /* Broadcast both directions in the chain to find insertion slot */
    (void)send_frame(LINK_PREDECESSOR, &req);
    (void)send_frame(LINK_SUCCESSOR, &req);
}

void wyresv2_init(wyresv2_ctx_t *ctx, uint16_t node_id)
{
    memset(ctx, 0, sizeof(*ctx));
    memset(s_pending, 0, sizeof(s_pending));

    ctx->node_id = node_id;
    ctx->predecessor_id = 0xFFFF;
    ctx->successor_id = 0xFFFF;
    ctx->state = NODE_OFF;
    ctx->next_seq = 1;

    set_led_for_state(ctx->state);
}

void wyresv2_power_on(wyresv2_ctx_t *ctx)
{
    ctx->state = NODE_BOOTING;
    ctx->inserted = false;
    set_led_for_state(ctx->state);

    /* Transition immediately to discovery for autonomous insertion. */
    ctx->state = NODE_DISCOVERING;
    set_led_for_state(ctx->state);
    discover_and_join(ctx);
}

void wyresv2_tick(wyresv2_ctx_t *ctx, uint32_t now_ms)
{
    uint32_t i;

    if (ctx->state == NODE_OFF) {
        return;
    }

    for (i = 0; i < WYRESV2_MAX_PENDING; ++i) {
        if (s_pending[i].used && now_ms >= s_pending[i].next_retry_ms) {
            if (s_pending[i].retries >= RETRANSMIT_MAX_ATTEMPTS) {
                s_pending[i].used = false;
                continue;
            }

            (void)send_frame(s_pending[i].dir, &s_pending[i].frame);
            s_pending[i].retries++;
            s_pending[i].next_retry_ms = now_ms + RETRANSMIT_TIMEOUT_MS;
        }
    }

    if (ctx->inserted) {
        if ((now_ms - ctx->last_heartbeat_ms) >= HEARTBEAT_PERIOD_MS) {
            wyresv2_frame_t hb;
            memset(&hb, 0, sizeof(hb));
            hb.version = FRAME_VERSION;
            hb.type = MSG_HEARTBEAT;
            hb.src_id = ctx->node_id;
            hb.dst_id = 0xFFFF;
            hb.seq = ctx->next_seq++;
            hb.ttl = 1;
            hb.flags = 0;
            hb.payload_len = 0;
            (void)send_frame(LINK_PREDECESSOR, &hb);
            (void)send_frame(LINK_SUCCESSOR, &hb);
            ctx->last_heartbeat_ms = now_ms;
        }

        if ((now_ms - ctx->last_heartbeat_ms) > HEARTBEAT_TIMEOUT_MS) {
            ctx->state = NODE_DISCOVERING;
            ctx->inserted = false;
            set_led_for_state(ctx->state);
            discover_and_join(ctx);
        }
    }
}

void wyresv2_on_frame(wyresv2_ctx_t *ctx, link_direction_t from, const wyresv2_frame_t *frame, int16_t rssi, int8_t snr)
{
    wyresv2_frame_t out;

    if (from == LINK_PREDECESSOR) {
        ctx->last_rssi_pred = rssi;
        ctx->last_snr_pred = snr;
    } else {
        ctx->last_rssi_succ = rssi;
        ctx->last_snr_succ = snr;
    }

    if (frame->type == MSG_ACK) {
        ack_pending(frame->seq);
        return;
    }

    if ((frame->flags & FRAME_FLAG_ACK_REQUIRED) != 0U) {
        (void)send_ack(ctx, from, frame->seq);
    }

    if (frame->type == MSG_JOIN_ACK) {
        ctx->inserted = true;
        ctx->state = NODE_INSERTED;
        set_led_for_state(ctx->state);
        return;
    }

    if (frame->type == MSG_ALERT) {
        ctx->state = NODE_ALERT_ACTIVE;
        set_led_for_state(ctx->state);

        out = *frame;
        (void)forward_with_ack(ctx, opposite_dir(from), &out, true);
        return;
    }

    if (frame->type == MSG_TEXT || frame->type == MSG_SUPERVISION || frame->type == MSG_HEARTBEAT || frame->type == MSG_JOIN_REQ) {
        out = *frame;
        (void)forward_with_ack(ctx, opposite_dir(from), &out, false);
    }
}

bool wyresv2_send_text(wyresv2_ctx_t *ctx, uint16_t dst_id, const uint8_t *payload, uint8_t len)
{
    wyresv2_frame_t f;

    if (len > WYRESV2_MAX_PAYLOAD) {
        return false;
    }

    memset(&f, 0, sizeof(f));
    f.version = FRAME_VERSION;
    f.type = MSG_TEXT;
    f.src_id = ctx->node_id;
    f.dst_id = dst_id;
    f.seq = ctx->next_seq++;
    f.ttl = DEFAULT_TTL;
    f.flags = FRAME_FLAG_ACK_REQUIRED;
    f.payload_len = len;
    if (len > 0U) {
        memcpy(f.payload, payload, len);
    }

    return forward_with_ack(ctx, LINK_SUCCESSOR, &f, false) || forward_with_ack(ctx, LINK_PREDECESSOR, &f, false);
}

bool wyresv2_send_alert(wyresv2_ctx_t *ctx, uint16_t dst_id, const uint8_t *payload, uint8_t len)
{
    wyresv2_frame_t f;

    if (len > WYRESV2_MAX_PAYLOAD) {
        return false;
    }

    memset(&f, 0, sizeof(f));
    f.version = FRAME_VERSION;
    f.type = MSG_ALERT;
    f.src_id = ctx->node_id;
    f.dst_id = dst_id;
    f.seq = ctx->next_seq++;
    f.ttl = DEFAULT_TTL;
    f.flags = FRAME_FLAG_ACK_REQUIRED;
    f.payload_len = len;
    if (len > 0U) {
        memcpy(f.payload, payload, len);
    }

    ctx->state = NODE_ALERT_ACTIVE;
    set_led_for_state(ctx->state);

    return forward_with_ack(ctx, LINK_SUCCESSOR, &f, true) || forward_with_ack(ctx, LINK_PREDECESSOR, &f, true);
}
