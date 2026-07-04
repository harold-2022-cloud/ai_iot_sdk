#pragma once

#include "ai_session_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ai_audio_tx_fn_t)(uint8_t *data, unsigned int size);

typedef struct {
    void (*register_tx)(ai_audio_tx_fn_t send_fn);
    int (*play_rx)(uint8_t *data, unsigned int len);
    void (*prepare_first_frame)(uint32_t session_id, const char *reason);
    void (*set_rx_playback_enabled)(bool enabled);
    void (*clear_rx_buffer)(const char *reason);
    void (*set_mic_uplink_enabled)(bool enabled);
} ai_audio_io_t;

/* Stub phase: returns NULL until the default ESP32-S3 audio bridge is wired. */
const ai_audio_io_t *Ai_Audio_Io_Get_Default(void);

#ifdef __cplusplus
}
#endif
