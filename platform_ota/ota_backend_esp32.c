#include "ota_backend_esp32.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include <string.h>

#define TAG "ota_backend_esp32"

typedef struct
{
    const esp_partition_t *partition;
    esp_ota_handle_t       handle;
    int                    first_packet;
    int                    begun;
} ota_esp32_state_t;

static ota_esp32_state_t s_state;

static int be_begin(void *ctx)
{
    (void)ctx;
    memset(&s_state, 0, sizeof(s_state));
    s_state.partition = esp_ota_get_next_update_partition(NULL);
    if (s_state.partition == NULL)
    {
        ESP_LOGE(TAG, "no OTA partition");
        return PLATFORM_OTA_ERR_BACKEND;
    }
    s_state.first_packet = 1;
    s_state.begun = 0;
    ESP_LOGI(TAG, "OTA begin: subtype=%d offset=0x%lx",
             s_state.partition->subtype, (unsigned long)s_state.partition->address);
    return PLATFORM_OTA_OK;
}

static int be_write(void *ctx, const void *data, size_t len, uint32_t total)
{
    (void)ctx;
    (void)total;
    esp_err_t err;
    if (s_state.first_packet)
    {
        err = esp_ota_begin(s_state.partition, OTA_WITH_SEQUENTIAL_WRITES, &s_state.handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
            return PLATFORM_OTA_ERR_BACKEND;
        }
        s_state.begun = 1;

        if (len < sizeof(esp_image_header_t))
        {
            ESP_LOGE(TAG, "first packet too short: len=%u", (unsigned)len);
            return PLATFORM_OTA_ERR_BACKEND;
        }
        const esp_image_header_t *hdr = (const esp_image_header_t *)data;
        if (hdr->chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID)
        {
            ESP_LOGE(TAG, "chip id mismatch: expected=%d found=%d", CONFIG_IDF_FIRMWARE_CHIP_ID, hdr->chip_id);
            return PLATFORM_OTA_ERR_BACKEND;
        }
        s_state.first_packet = 0;
    }
    err = esp_ota_write(s_state.handle, data, len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_write: 0x%x len=%u", err, (unsigned)len);
        return PLATFORM_OTA_ERR_BACKEND;
    }
    return PLATFORM_OTA_OK;
}

static int be_commit(void *ctx)
{
    (void)ctx;
    esp_err_t err;
    if (!s_state.begun || s_state.partition == NULL)
    {
        ESP_LOGE(TAG, "commit before begin");
        return PLATFORM_OTA_ERR_BACKEND;
    }
    err = esp_ota_end(s_state.handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end: 0x%x", err);
        return PLATFORM_OTA_ERR_BACKEND;
    }
    s_state.begun = 0;
    err = esp_ota_set_boot_partition(s_state.partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set_boot_partition: 0x%x", err);
        return PLATFORM_OTA_ERR_BACKEND;
    }
    ESP_LOGI(TAG, "OTA commit: boot set to subtype=%d", s_state.partition->subtype);
    return PLATFORM_OTA_OK;
}

static int be_abort(void *ctx)
{
    (void)ctx;
    if (s_state.begun)
    {
        esp_ota_abort(s_state.handle);
        s_state.begun = 0;
    }
    memset(&s_state, 0, sizeof(s_state));
    return PLATFORM_OTA_OK;
}

static const platform_ota_backend_ops_t s_esp32_ops = {
    .begin = be_begin,
    .write = be_write,
    .commit = be_commit,
    .abort = be_abort,
};

const platform_ota_backend_ops_t *ota_backend_esp32_ops(void)
{
    return &s_esp32_ops;
}

void *ota_backend_esp32_ctx(void)
{
    return NULL;
}
