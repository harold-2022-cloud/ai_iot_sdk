//entity_uart.h
#pragma once

#include <inttypes.h>


#define ENTITY_UART_BAUD      115200

typedef struct 
{
    unsigned short Rx_Buf_Size;
    void (*User_Uart_Init)(uint32_t baudrate);
    void (*User_Uart_Deinit)(void);
    int (*User_Uart_Write_Bytes)(uint8_t *pdata, uint16_t len);
    void (*User_Uart_Register_Rec_Cbs)(void* cb);
}Entity_Uart_Cbs_t;




//Uart板载接口函数初始化
void Entity_Uart_Cbs_Init(Entity_Uart_Cbs_t *cbs);

//Uart授权接口初始化
void Entity_Uart_Auth_Init(void);






