#include "ai_rtc_agora_datastream.h"

#include "cJSON.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define AI_RTC_AGORA_DS_MAX_SPLIT 4u
#define AI_RTC_AGORA_DS_MAX_ENCODED_LEN (1024u * 8u)
#define AI_RTC_AGORA_DS_PACKET_CAP (AI_RTC_AGORA_DS_MAX_ENCODED_LEN + 96u)
#define AI_RTC_AGORA_DS_DECODE_MAX (((AI_RTC_AGORA_DS_MAX_ENCODED_LEN / 4u) * 3u) + 1u)
#define AI_RTC_AGORA_DS_MAX_MSG_ID_LEN 63u

typedef struct
{
    bool active;
    char msg_id[AI_RTC_AGORA_DS_MAX_MSG_ID_LEN + 1u];
    uint8_t expected_total;
    uint8_t next_index;
    size_t encoded_len;
    char encoded[AI_RTC_AGORA_DS_MAX_ENCODED_LEN + 1u];
    char decoded[AI_RTC_AGORA_DS_DECODE_MAX];
} Ai_Rtc_Agora_Datastream_Context_t;

static Ai_Rtc_Agora_Datastream_Context_t s_datastream;
static char s_packet[AI_RTC_AGORA_DS_PACKET_CAP];

static int base64_value(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z')
    {
        return ch - 'a' + 26;
    }
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0' + 52;
    }
    if (ch == '+')
    {
        return 62;
    }
    if (ch == '/')
    {
        return 63;
    }
    if (ch == '=')
    {
        return 0;
    }
    if (ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ')
    {
        return -2;
    }
    return -1;
}

static int base64_decode(const char *src, size_t src_len, char *out, size_t out_cap, size_t *out_len)
{
    size_t i;
    size_t pos = 0u;
    int block[4];
    char chars[4];
    size_t block_len = 0u;

    if (src == NULL || out == NULL || out_cap == 0u || out_len == NULL)
    {
        return -1;
    }

    for (i = 0u; i < src_len; ++i)
    {
        int value = base64_value(src[i]);

        if (value == -2)
        {
            continue;
        }
        if (value < 0)
        {
            return -1;
        }
        block[block_len] = value;
        chars[block_len] = src[i];
        block_len++;

        if (block_len == 4u)
        {
            unsigned int triple = ((unsigned int)block[0] << 18) |
                                  ((unsigned int)block[1] << 12) |
                                  ((unsigned int)block[2] << 6) |
                                  (unsigned int)block[3];

            if (pos >= out_cap)
            {
                return -1;
            }
            out[pos++] = (char)((triple >> 16) & 0xFFu);
            if (chars[2] != '=')
            {
                if (pos >= out_cap)
                {
                    return -1;
                }
                out[pos++] = (char)((triple >> 8) & 0xFFu);
            }
            if (chars[3] != '=')
            {
                if (pos >= out_cap)
                {
                    return -1;
                }
                out[pos++] = (char)(triple & 0xFFu);
            }
            block_len = 0u;
        }
    }

    if (block_len != 0u || pos >= out_cap)
    {
        return -1;
    }
    out[pos] = '\0';
    *out_len = pos;
    return 0;
}

static int parse_uint8_field(const char *text, uint8_t *out)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || text[0] == '\0' || out == NULL)
    {
        return -1;
    }
    value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value > 255u)
    {
        return -1;
    }
    *out = (uint8_t)value;
    return 0;
}

static Ai_Rtc_Agora_Datastream_State_t parse_state(const char *state)
{
    if (state == NULL)
    {
        return AI_RTC_AGORA_DS_STATE_UNKNOWN;
    }
    if (strcmp(state, "listening") == 0)
    {
        return AI_RTC_AGORA_DS_STATE_LISTENING;
    }
    if (strcmp(state, "thinking") == 0)
    {
        return AI_RTC_AGORA_DS_STATE_THINKING;
    }
    if (strcmp(state, "speaking") == 0)
    {
        return AI_RTC_AGORA_DS_STATE_SPEAKING;
    }
    if (strcmp(state, "silent") == 0)
    {
        return AI_RTC_AGORA_DS_STATE_SILENT;
    }
    return AI_RTC_AGORA_DS_STATE_UNKNOWN;
}

static void emit_json_event(const char *json,
                            size_t json_len,
                            Ai_Rtc_Agora_Datastream_Event_Cb cb,
                            void *user)
{
    Ai_Rtc_Agora_Datastream_Event_t event;
    cJSON *root;
    cJSON *object;

    if (cb == NULL)
    {
        return;
    }

    memset(&event, 0, sizeof(event));
    event.raw_json = json;
    event.raw_json_len = json_len;

    root = cJSON_Parse(json);
    if (root == NULL)
    {
        cb(&event, user);
        return;
    }

    object = cJSON_GetObjectItem(root, "object");
    if (cJSON_IsString(object) && object->valuestring != NULL)
    {
        if (strcmp(object->valuestring, "message.state") == 0)
        {
            cJSON *state = cJSON_GetObjectItem(root, "state");

            event.object = AI_RTC_AGORA_DS_OBJECT_MESSAGE_STATE;
            if (cJSON_IsString(state))
            {
                event.state = parse_state(state->valuestring);
            }
        }
        else if (strcmp(object->valuestring, "message.user") == 0)
        {
            event.object = AI_RTC_AGORA_DS_OBJECT_MESSAGE_USER;
        }
    }

    cb(&event, user);
    cJSON_Delete(root);
}

static int split_packet(char *packet,
                        char **msg_id,
                        char **cur_index,
                        char **total_num,
                        char **payload)
{
    char *cursor = packet;
    char *sep;

    *msg_id = cursor;
    sep = strchr(cursor, '|');
    if (sep == NULL)
    {
        return -1;
    }
    *sep = '\0';
    cursor = sep + 1;

    *cur_index = cursor;
    sep = strchr(cursor, '|');
    if (sep == NULL)
    {
        return -1;
    }
    *sep = '\0';
    cursor = sep + 1;

    *total_num = cursor;
    sep = strchr(cursor, '|');
    if (sep == NULL)
    {
        return -1;
    }
    *sep = '\0';
    *payload = sep + 1;
    return 0;
}

void Ai_Rtc_Agora_Datastream_Reset(void)
{
    memset(&s_datastream, 0, sizeof(s_datastream));
}

int Ai_Rtc_Agora_Datastream_On_Message(const uint8_t *data,
                                       size_t len,
                                       Ai_Rtc_Agora_Datastream_Event_Cb cb,
                                       void *user)
{
    char *packet = s_packet;
    char *msg_id = NULL;
    char *cur_index_str = NULL;
    char *total_num_str = NULL;
    char *payload = NULL;
    uint8_t cur_index;
    uint8_t total_num;
    size_t msg_id_len;
    size_t payload_len;
    size_t decode_len = 0;

    if (data == NULL || len == 0u || len >= AI_RTC_AGORA_DS_PACKET_CAP)
    {
        Ai_Rtc_Agora_Datastream_Reset();
        return -1;
    }

    memcpy(packet, data, len);
    packet[len] = '\0';
    if (split_packet(packet, &msg_id, &cur_index_str, &total_num_str, &payload) != 0 ||
        parse_uint8_field(cur_index_str, &cur_index) != 0 ||
        parse_uint8_field(total_num_str, &total_num) != 0)
    {
        Ai_Rtc_Agora_Datastream_Reset();
        return -1;
    }

    msg_id_len = strlen(msg_id);
    payload_len = strlen(payload);
    if (msg_id_len == 0u || msg_id_len > AI_RTC_AGORA_DS_MAX_MSG_ID_LEN ||
        total_num == 0u || total_num > AI_RTC_AGORA_DS_MAX_SPLIT ||
        cur_index == 0u || cur_index > total_num)
    {
        Ai_Rtc_Agora_Datastream_Reset();
        return -1;
    }

    if (!s_datastream.active ||
        strcmp(s_datastream.msg_id, msg_id) != 0 ||
        cur_index == 1u)
    {
        Ai_Rtc_Agora_Datastream_Reset();
        s_datastream.active = true;
        memcpy(s_datastream.msg_id, msg_id, msg_id_len + 1u);
        s_datastream.expected_total = total_num;
        s_datastream.next_index = 1u;
    }

    if (!s_datastream.active ||
        s_datastream.expected_total != total_num ||
        cur_index != s_datastream.next_index ||
        payload_len > AI_RTC_AGORA_DS_MAX_ENCODED_LEN - s_datastream.encoded_len)
    {
        Ai_Rtc_Agora_Datastream_Reset();
        return -1;
    }

    memcpy(s_datastream.encoded + s_datastream.encoded_len, payload, payload_len);
    s_datastream.encoded_len += payload_len;
    s_datastream.encoded[s_datastream.encoded_len] = '\0';
    s_datastream.next_index++;

    if (cur_index != total_num)
    {
        return 0;
    }

    if (base64_decode(s_datastream.encoded,
                      s_datastream.encoded_len,
                      s_datastream.decoded,
                      sizeof(s_datastream.decoded),
                      &decode_len) != 0)
    {
        Ai_Rtc_Agora_Datastream_Reset();
        return -1;
    }

    emit_json_event(s_datastream.decoded, decode_len, cb, user);
    Ai_Rtc_Agora_Datastream_Reset();
    return 1;
}
