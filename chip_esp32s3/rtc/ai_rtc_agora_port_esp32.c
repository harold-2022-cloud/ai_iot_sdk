#include "ai_rtc_agora_port.h"

#include "bsp_system.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef AI_RTC_AGORA_ESP32_TASK_CORE
#define AI_RTC_AGORA_ESP32_TASK_CORE 0
#endif

static bool s_network_registered;
static bool s_network_online;
static Ai_Rtc_Agora_Port_Network_Cb s_network_cb;
static void *s_network_user;
static esp_event_handler_instance_t s_wifi_connected_inst;
static esp_event_handler_instance_t s_wifi_disconnected_inst;
static esp_event_handler_instance_t s_wifi_beacon_timeout_inst;
static esp_event_handler_instance_t s_ip_got_ip_inst;
static esp_event_handler_instance_t s_ip_lost_ip_inst;

static void esp_network_unregister(void);

static void esp_network_notify(Ai_Rtc_Agora_Port_Network_Event_t event)
{
    if (s_network_cb != NULL)
    {
        s_network_cb(event, s_network_user);
    }
}

static void esp_network_set_down(Ai_Rtc_Agora_Port_Network_Event_t event)
{
    s_network_online = false;
    esp_network_notify(event);
}

static void esp_network_event_handler(void *arg,
                                      esp_event_base_t event_base,
                                      int32_t event_id,
                                      void *event_data)
{
    (void)arg;

    if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            bool was_online = s_network_online;
            bool ip_changed = (event != NULL) ? event->ip_changed : false;

            s_network_online = true;

            esp_network_notify((was_online || ip_changed)
                                   ? AI_RTC_AGORA_PORT_NETWORK_CHANGE
                                   : AI_RTC_AGORA_PORT_NETWORK_UP);
        }
        else if (event_id == IP_EVENT_STA_LOST_IP)
        {
            esp_network_set_down(AI_RTC_AGORA_PORT_NETWORK_DOWN);
        }
        return;
    }

    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_DISCONNECTED || event_id == WIFI_EVENT_STA_BEACON_TIMEOUT)
        {
            esp_network_set_down(AI_RTC_AGORA_PORT_NETWORK_DOWN);
        }
    }
}

static int esp_network_register(Ai_Rtc_Agora_Port_Network_Cb cb, void *user)
{
    s_network_cb = cb;
    s_network_user = user;

    if (s_network_registered)
    {
        return 0;
    }

    esp_err_t ret = esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_STA_CONNECTED,
                                                        esp_network_event_handler,
                                                        NULL,
                                                        &s_wifi_connected_inst);
    if (ret != ESP_OK)
    {
        esp_network_unregister();
        return (int)ret;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                              WIFI_EVENT_STA_DISCONNECTED,
                                              esp_network_event_handler,
                                              NULL,
                                              &s_wifi_disconnected_inst);
    if (ret != ESP_OK)
    {
        esp_network_unregister();
        return (int)ret;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                              WIFI_EVENT_STA_BEACON_TIMEOUT,
                                              esp_network_event_handler,
                                              NULL,
                                              &s_wifi_beacon_timeout_inst);
    if (ret != ESP_OK)
    {
        esp_network_unregister();
        return (int)ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              esp_network_event_handler,
                                              NULL,
                                              &s_ip_got_ip_inst);
    if (ret != ESP_OK)
    {
        esp_network_unregister();
        return (int)ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_LOST_IP,
                                              esp_network_event_handler,
                                              NULL,
                                              &s_ip_lost_ip_inst);
    if (ret != ESP_OK)
    {
        esp_network_unregister();
        return (int)ret;
    }

    s_network_registered = true;
    return 0;
}

static void esp_network_unregister_one(esp_event_base_t event_base,
                                       int32_t event_id,
                                       esp_event_handler_instance_t *instance)
{
    if (*instance != NULL)
    {
        (void)esp_event_handler_instance_unregister(event_base, event_id, *instance);
        *instance = NULL;
    }
}

static void esp_network_unregister(void)
{
    esp_network_unregister_one(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &s_wifi_connected_inst);
    esp_network_unregister_one(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &s_wifi_disconnected_inst);
    esp_network_unregister_one(WIFI_EVENT, WIFI_EVENT_STA_BEACON_TIMEOUT, &s_wifi_beacon_timeout_inst);
    esp_network_unregister_one(IP_EVENT, IP_EVENT_STA_GOT_IP, &s_ip_got_ip_inst);
    esp_network_unregister_one(IP_EVENT, IP_EVENT_STA_LOST_IP, &s_ip_lost_ip_inst);

    s_network_registered = false;
    s_network_online = false;
    s_network_cb = NULL;
    s_network_user = NULL;
}

static int esp_network_refresh(void)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info = {0};

    if ((sta != NULL) && (esp_netif_get_ip_info(sta, &ip_info) == ESP_OK) && (ip_info.ip.addr != 0))
    {
        s_network_online = true;
        esp_network_notify(AI_RTC_AGORA_PORT_NETWORK_UP);
    }
    else
    {
        esp_network_set_down(AI_RTC_AGORA_PORT_NETWORK_DOWN);
    }

    return 0;
}

static int esp_sema_create(Ai_Rtc_Agora_Port_Sema_t *out, int max_count)
{
    return (int)Bsp_Semaphore_Init((Bsp_Sema_t *)out, max_count);
}

static void esp_sema_destroy(Ai_Rtc_Agora_Port_Sema_t *sema)
{
    (void)Bsp_Semaphore_Deinit((Bsp_Sema_t *)sema);
}

static int esp_sema_give(Ai_Rtc_Agora_Port_Sema_t *sema)
{
    return (int)Bsp_Set_Semaphore((Bsp_Sema_t *)sema);
}

static int esp_sema_take(Ai_Rtc_Agora_Port_Sema_t *sema, uint32_t timeout_ms)
{
    return (int)Bsp_Get_Semaphore((Bsp_Sema_t *)sema, timeout_ms);
}

static int esp_queue_create(Ai_Rtc_Agora_Port_Queue_t *out, uint16_t depth, uint32_t item_size)
{
    return (int)Bsp_Msg_Queue_Create((Bsp_Queue_t *)out, depth, item_size);
}

static void esp_queue_destroy(Ai_Rtc_Agora_Port_Queue_t *queue)
{
    (void)Bsp_Msg_Queue_Delete((Bsp_Queue_t *)queue);
}

static int esp_queue_send(Ai_Rtc_Agora_Port_Queue_t *queue,
                          const void *item,
                          uint32_t item_size,
                          uint32_t timeout_ms)
{
    return (int)Bsp_Msg_Queue_Send((Bsp_Queue_t *)queue, (void *)item, item_size, timeout_ms);
}

static int esp_queue_recv(Ai_Rtc_Agora_Port_Queue_t *queue,
                          void *item,
                          uint32_t *item_size,
                          uint32_t timeout_ms)
{
    return (int)Bsp_Msg_Queue_Wait((Bsp_Queue_t *)queue, item, item_size, timeout_ms);
}

static int esp_task_create(Ai_Rtc_Agora_Port_Task_t *out,
                           const char *name,
                           uint32_t stack_size,
                           uint32_t priority,
                           void (*entry)(void *),
                           void *arg)
{
    return Bsp_Pthread_Create_Ex((Bsp_Thread_t *)out,
                                 name,
                                 stack_size,
                                 priority,
                                 entry,
                                 arg,
                                 AI_RTC_AGORA_ESP32_TASK_CORE,
                                 BSP_MEM_PSRAM);
}

static void esp_task_delete(Ai_Rtc_Agora_Port_Task_t *task)
{
    (void)Bsp_Pthread_Psram_Delete((Bsp_Thread_t *)task);
}

static void *esp_mem_alloc(size_t size)
{
    return Bsp_Psram_Malloc((unsigned int)size);
}

static void *esp_mem_zalloc(size_t size)
{
    return Bsp_Psram_Zalloc((unsigned int)size);
}

static void esp_mem_free(void *ptr)
{
    Bsp_Psram_Free(ptr);
}

static void esp_sleep_ms(uint32_t ms)
{
    Bsp_Sleep_Ms(ms);
}

static uint32_t esp_timestamp_ms(void)
{
    return (uint32_t)Bsp_Get_Time_Stamp();
}

static void esp_debug_heap(const char *stage)
{
    (void)stage;
    Bsp_Debug_Heap_Info();
}

static const Ai_Rtc_Agora_Port_t s_esp32_port = {
    .sema_create = esp_sema_create,
    .sema_destroy = esp_sema_destroy,
    .sema_give = esp_sema_give,
    .sema_take = esp_sema_take,
    .queue_create = esp_queue_create,
    .queue_destroy = esp_queue_destroy,
    .queue_send = esp_queue_send,
    .queue_recv = esp_queue_recv,
    .task_create = esp_task_create,
    .task_delete = esp_task_delete,
    .mem_alloc = esp_mem_alloc,
    .mem_zalloc = esp_mem_zalloc,
    .mem_free = esp_mem_free,
    .sleep_ms = esp_sleep_ms,
    .timestamp_ms = esp_timestamp_ms,
    .debug_heap = esp_debug_heap,
    .network_register = esp_network_register,
    .network_unregister = esp_network_unregister,
    .network_refresh = esp_network_refresh,
};

const Ai_Rtc_Agora_Port_t *Ai_Rtc_Agora_Port_Get(void)
{
    return &s_esp32_port;
}
