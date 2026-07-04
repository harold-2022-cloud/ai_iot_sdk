#ifndef TEST_STUB_AGORA_RTC_API_H
#define TEST_STUB_AGORA_RTC_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    ERR_NET_DOWN = 14,
} agora_err_code_e;

typedef enum
{
    AUDIO_DATA_TYPE_OPUS = 1,
    AUDIO_DATA_TYPE_G722 = 5,
    AUDIO_DATA_TYPE_PCM = 100,
} audio_data_type_e;

typedef enum
{
    AUDIO_CODEC_DISABLED = 0,
    AUDIO_CODEC_TYPE_OPUS = 1,
    AUDIO_CODEC_TYPE_G722 = 2,
} audio_codec_type_e;

typedef enum
{
    VIDEO_DATA_TYPE_H264 = 2,
    VIDEO_DATA_TYPE_GENERIC_JPEG = 20,
} video_data_type_e;

typedef enum
{
    VIDEO_FRAME_AUTO_DETECT = 0,
    VIDEO_FRAME_KEY = 3,
    VIDEO_FRAME_DELTA = 4,
} video_frame_type_e;

typedef enum
{
    VIDEO_STREAM_HIGH = 0,
    VIDEO_STREAM_LOW = 1,
} video_stream_type_e;

typedef enum
{
    VIDEO_ORIENTATION_0 = 0,
} video_orientation_e;

typedef uint16_t video_frame_rate_e;

typedef enum
{
    AREA_CODE_GLOB = -1,
} area_code_e;

typedef enum
{
    NETWORK_EVENT_DOWN = 0,
    NETWORK_EVENT_UP,
    NETWORK_EVENT_CHANGE,
} network_event_e;

typedef enum
{
    RTM_EVENT_TYPE_LOGIN = 0,
    RTM_EVENT_TYPE_KICKOFF = 1,
    RTM_EVENT_TYPE_EXIT = 2,
} rtm_event_type_e;

typedef enum
{
    RTM_MSG_STATE_INIT = 0,
    RTM_MSG_STATE_RECEIVED,
    RTM_MSG_STATE_UNREACHABLE,
    RTM_MSG_STATE_TIMEOUT,
} rtm_msg_state_e;

typedef enum
{
    ERR_RTM_OK = 0,
    ERR_RTM_FAILED = 1,
} rtm_err_code_e;

typedef enum
{
    RTC_LOG_WARNING = 6,
} rtc_log_level_e;

typedef struct
{
    audio_data_type_e data_type;
} audio_frame_info_t;

typedef struct
{
    video_data_type_e data_type;
    video_stream_type_e stream_type;
    video_frame_type_e frame_type;
    video_frame_rate_e frame_rate;
    video_orientation_e rotation;
} video_frame_info_t;

typedef struct
{
    bool log_disable;
    rtc_log_level_e log_level;
} log_config_t;

typedef struct
{
    uint32_t area_code;
    log_config_t log_cfg;
    bool use_string_uid;
} rtc_service_option_t;

typedef struct
{
    audio_codec_type_e audio_codec_type;
    int pcm_sample_rate;
    int pcm_channel_num;
    int pcm_duration;
} audio_codec_option_t;

typedef struct
{
    bool auto_subscribe_audio;
    bool auto_subscribe_video;
    bool enable_audio_jitter_buffer;
    bool enable_audio_downlink_aec;
    bool enable_audio_ai_qos;
    bool enable_audio_decode;
    audio_codec_option_t audio_codec_opt;
} rtc_channel_options_t;

typedef uint32_t connection_id_t;

#define CONNECTION_ID_INVALID ((connection_id_t)-1)

typedef struct
{
    void (*on_error)(connection_id_t conn_id, int code, const char *msg);
    void (*on_join_channel_success)(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    void (*on_reconnecting)(connection_id_t conn_id);
    void (*on_connection_lost)(connection_id_t conn_id);
    void (*on_rejoin_channel_success)(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    void (*on_token_privilege_will_expire)(connection_id_t conn_id, const char *token);
    void (*on_user_joined)(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    void (*on_user_offline)(connection_id_t conn_id, uint32_t uid, int reason);
    void (*on_audio_data)(connection_id_t conn_id,
                          uint32_t uid,
                          uint16_t sent_ts,
                          const void *data_ptr,
                          size_t data_len,
                          const audio_frame_info_t *info_ptr);
    void (*on_video_data)(connection_id_t conn_id,
                          uint32_t uid,
                          uint16_t sent_ts,
                          const void *data_ptr,
                          size_t data_len,
                          const video_frame_info_t *info_ptr);
    void (*on_target_bitrate_changed)(connection_id_t conn_id, uint32_t target_bps);
    void (*on_key_frame_gen_req)(connection_id_t conn_id, uint32_t uid, video_stream_type_e stream_type);
    void (*on_stream_message)(connection_id_t conn_id,
                              uint32_t uid,
                              int stream_id,
                              const char *data,
                              size_t length,
                              uint64_t sent_ts);
} agora_rtc_event_handler_t;

typedef struct
{
    void (*on_rtm_event)(const char *rtm_uid, rtm_event_type_e event_type, rtm_err_code_e err_code);
    void (*on_rtm_data)(const char *rtm_uid, const void *msg, size_t msg_len, const char *custom_type);
    void (*on_rtm_send_data_result)(const char *rtm_uid, uint32_t msg_id, rtm_msg_state_e state);
} agora_rtm_handler_t;

const char *agora_rtc_get_version(void);
const char *agora_rtc_err_2_str(int err);
int agora_rtc_init(const char *app_id, const agora_rtc_event_handler_t *event_handler, rtc_service_option_t *option);
int agora_rtc_fini(void);
int agora_rtc_create_connection(connection_id_t *conn_id);
int agora_rtc_destroy_connection(connection_id_t conn_id);
int agora_rtc_join_channel(connection_id_t conn_id,
                           const char *channel_name,
                           uint32_t uid,
                           const char *token,
                           rtc_channel_options_t *options);
int agora_rtc_join_channel_with_user_account(connection_id_t conn_id,
                                             const char *channel_name,
                                             const char *user_account,
                                             const char *token,
                                             rtc_channel_options_t *options);
int agora_rtc_leave_channel(connection_id_t conn_id);
int agora_rtc_renew_token(connection_id_t conn_id, const char *token);
int agora_rtc_send_audio_data(connection_id_t conn_id,
                              const void *data_ptr,
                              size_t data_len,
                              audio_frame_info_t *info_ptr);
int agora_rtc_send_video_data(connection_id_t conn_id,
                              const void *data_ptr,
                              size_t data_len,
                              video_frame_info_t *info_ptr);
int agora_rtc_create_data_stream(connection_id_t conn_id, int *stream_id, bool reliable, bool ordered);
int agora_rtc_send_stream_message(connection_id_t conn_id, int stream_id, const char *data, size_t length);
int agora_rtc_login_rtm(const char *rtm_uid, const char *rtm_token, const agora_rtm_handler_t *handler);
int agora_rtc_logout_rtm(void);
int agora_rtc_send_rtm_data(const char *rtm_uid,
                            const void *msg,
                            size_t msg_len,
                            uint32_t msg_id,
                            const char *custom_type);
int agora_rtc_notify_network_event(network_event_e event);

typedef struct
{
    int init_calls;
    int fini_calls;
    int create_connection_calls;
    int join_channel_calls;
    int join_channel_with_user_account_calls;
    int create_data_stream_calls;
    int send_audio_calls;
    int send_video_calls;
    int send_stream_calls;
    int login_rtm_calls;
    int logout_rtm_calls;
    int send_rtm_calls;
    int leave_channel_calls;
    int destroy_connection_calls;
    int renew_token_calls;
    int notify_network_calls;
    int target_bitrate_callbacks;
    int key_frame_callbacks;
    connection_id_t conn_id;
    connection_id_t last_join_conn_id;
    connection_id_t last_leave_conn_id;
    connection_id_t last_destroy_conn_id;
    int stream_id;
    const char *last_app_id;
    const char *last_channel_name;
    const char *last_token;
    const char *last_renew_token;
    const char *last_user_account;
    uint32_t last_uid;
    bool last_reliable;
    bool last_ordered;
    bool last_auto_subscribe_audio;
    bool last_auto_subscribe_video;
    bool last_enable_audio_jitter_buffer;
    bool last_enable_audio_downlink_aec;
    bool last_enable_audio_ai_qos;
    bool last_enable_audio_decode;
    audio_codec_type_e last_audio_codec_type;
    int last_pcm_sample_rate;
    int last_pcm_channel_num;
    int last_pcm_duration;
    network_event_e last_network_event;
    audio_data_type_e last_audio_type;
    const void *last_audio_data;
    size_t last_audio_len;
    video_data_type_e last_video_type;
    video_frame_type_e last_video_frame_type;
    video_stream_type_e last_video_stream_type;
    const void *last_video_data;
    size_t last_video_len;
    video_frame_rate_e last_video_frame_rate;
    uint32_t last_target_bitrate;
    const char *last_stream_data;
    size_t last_stream_len;
    const char *last_rtm_uid;
    const char *last_rtm_token;
    const char *last_rtm_peer_uid;
    const void *last_rtm_data;
    size_t last_rtm_len;
    uint32_t last_rtm_msg_id;
} Agora_Rtc_Stub_State_t;

void Agora_Rtc_Stub_Reset(void);
Agora_Rtc_Stub_State_t *Agora_Rtc_Stub_State(void);
void Agora_Rtc_Stub_Emit_Joined(void);
void Agora_Rtc_Stub_Emit_Joined_For_Conn(connection_id_t conn_id);
void Agora_Rtc_Stub_Emit_Reconnecting(void);
void Agora_Rtc_Stub_Emit_Reconnecting_For_Conn(connection_id_t conn_id);
void Agora_Rtc_Stub_Emit_Rejoined(void);
void Agora_Rtc_Stub_Emit_Rejoined_For_Conn(connection_id_t conn_id);
void Agora_Rtc_Stub_Emit_User_Joined(uint32_t uid);
void Agora_Rtc_Stub_Emit_User_Joined_For_Conn(connection_id_t conn_id, uint32_t uid);
void Agora_Rtc_Stub_Emit_User_Offline(uint32_t uid);
void Agora_Rtc_Stub_Emit_User_Offline_For_Conn(connection_id_t conn_id, uint32_t uid);
void Agora_Rtc_Stub_Emit_Token_Will_Expire(void);
void Agora_Rtc_Stub_Emit_Token_Will_Expire_For_Conn(connection_id_t conn_id);
void Agora_Rtc_Stub_Emit_Audio(const void *data, size_t len, audio_data_type_e type);
void Agora_Rtc_Stub_Emit_Audio_For_Conn(connection_id_t conn_id,
                                        const void *data,
                                        size_t len,
                                        audio_data_type_e type);
void Agora_Rtc_Stub_Emit_Video(const void *data, size_t len, video_data_type_e type, video_frame_type_e frame_type);
void Agora_Rtc_Stub_Emit_Video_For_Conn(connection_id_t conn_id,
                                        const void *data,
                                        size_t len,
                                        video_data_type_e type,
                                        video_frame_type_e frame_type);
void Agora_Rtc_Stub_Emit_Target_Bitrate(uint32_t target_bitrate);
void Agora_Rtc_Stub_Emit_Key_Frame_Request(uint32_t uid);
void Agora_Rtc_Stub_Emit_Stream_Message(int stream_id, uint32_t uid, const char *data, size_t len, uint64_t sent_ts);
void Agora_Rtc_Stub_Emit_Stream_Message_For_Conn(connection_id_t conn_id,
                                                 int stream_id,
                                                 uint32_t uid,
                                                 const char *data,
                                                 size_t len,
                                                 uint64_t sent_ts);
void Agora_Rtc_Stub_Emit_Rtm_Login_Ok(void);
void Agora_Rtc_Stub_Emit_Rtm_Data(const char *rtm_uid, const void *msg, size_t msg_len);

#endif /* TEST_STUB_AGORA_RTC_API_H */
