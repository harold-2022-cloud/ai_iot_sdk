#ifndef AI_RTC_AGORA_PORT_H
#define AI_RTC_AGORA_PORT_H

#include <stddef.h>
#include <stdint.h>

typedef void *Ai_Rtc_Agora_Port_Sema_t;
typedef void *Ai_Rtc_Agora_Port_Queue_t;
typedef void *Ai_Rtc_Agora_Port_Task_t;

typedef enum
{
    AI_RTC_AGORA_PORT_NETWORK_DOWN = 0,
    AI_RTC_AGORA_PORT_NETWORK_UP,
    AI_RTC_AGORA_PORT_NETWORK_CHANGE,
} Ai_Rtc_Agora_Port_Network_Event_t;

typedef void (*Ai_Rtc_Agora_Port_Network_Cb)(Ai_Rtc_Agora_Port_Network_Event_t event,
                                             void *user);

typedef struct
{
    int (*sema_create)(Ai_Rtc_Agora_Port_Sema_t *out, int max_count);
    void (*sema_destroy)(Ai_Rtc_Agora_Port_Sema_t *sema);
    int (*sema_give)(Ai_Rtc_Agora_Port_Sema_t *sema);
    int (*sema_take)(Ai_Rtc_Agora_Port_Sema_t *sema, uint32_t timeout_ms);

    int (*queue_create)(Ai_Rtc_Agora_Port_Queue_t *out, uint16_t depth, uint32_t item_size);
    void (*queue_destroy)(Ai_Rtc_Agora_Port_Queue_t *queue);
    int (*queue_send)(Ai_Rtc_Agora_Port_Queue_t *queue,
                      const void *item,
                      uint32_t item_size,
                      uint32_t timeout_ms);
    int (*queue_recv)(Ai_Rtc_Agora_Port_Queue_t *queue,
                      void *item,
                      uint32_t *item_size,
                      uint32_t timeout_ms);

    int (*task_create)(Ai_Rtc_Agora_Port_Task_t *out,
                       const char *name,
                       uint32_t stack_size,
                       uint32_t priority,
                       void (*entry)(void *),
                       void *arg);
    void (*task_delete)(Ai_Rtc_Agora_Port_Task_t *task);

    void *(*mem_alloc)(size_t size);
    void *(*mem_zalloc)(size_t size);
    void (*mem_free)(void *ptr);

    void (*sleep_ms)(uint32_t ms);
    uint32_t (*timestamp_ms)(void);
    void (*debug_heap)(const char *stage);

    int (*network_register)(Ai_Rtc_Agora_Port_Network_Cb cb, void *user);
    void (*network_unregister)(void);
    int (*network_refresh)(void);
} Ai_Rtc_Agora_Port_t;

const Ai_Rtc_Agora_Port_t *Ai_Rtc_Agora_Port_Get(void);

#endif /* AI_RTC_AGORA_PORT_H */
