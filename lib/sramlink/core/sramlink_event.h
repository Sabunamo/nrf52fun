#pragma once

#include <stdint.h>

/** Event type that triggered the callback */
typedef enum sl_evt
{
    /** All originally intended packets were transmitted, so tx of the message is complete */
    SL_EVT_TX_COMPLETE,

    /** Another transmission of equal priority stopped the current transmission before completion */
    SL_EVT_TX_CLOBBERED,

    /** Another transmission of higher priority stopped the current transmission before completion */
    SL_EVT_TX_HIGHER_PRIORITY,

    SL_EVT_INVALID = 255
} sl_evt_t;

typedef void (* sl_evt_cb_t)(
    sl_evt_t event, uint16_t tx_packet_count,
    void* data
    );
