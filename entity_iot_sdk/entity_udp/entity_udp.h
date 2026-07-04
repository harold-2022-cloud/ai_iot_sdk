//entity_udp.h
#pragma once



#define UDP_BOARDCAST_TASK_STACK_SIZE       4096
#define UDP_BOARDCAST_TASK_PROI             3

#define UDP_SERVER_TASK_STACK_SIZE          4096
#define UDP_SERVER_TASK_PROI                3
#define UDP_SERVER_RECV_BUF_SIZE            2048


typedef struct 
{
    void *Thread_Id;
    int Socket_Id;
    char *Context;
    unsigned int Context_Len;
    int Task_Runing;
    int Boardcast_Addr;
    unsigned short Boardcast_Port;
}Udp_Boradcast_Task_Info_t;

typedef struct 
{
    void *Thread_Id;
    int Socket_Id;
    int Task_Runing;
    unsigned short Server_Port;
    unsigned char Client_Connected;
    int (*Recv_Callback)(const char *recv_buf);
    int (*Send_Bytes)(char *pdata, unsigned int len);
}Udp_Server_Task_Info_t;

//启动UDP广播任务
int Entity_Udp_Broadcast_Task_Start(Udp_Boradcast_Task_Info_t *info);

//停止UDP广播任务
int Entity_Udp_Broadcast_Task_Stop(Udp_Boradcast_Task_Info_t *info);

//启动UDP服务端监听任务
int Entity_Udp_Server_Task_Start(Udp_Server_Task_Info_t *info);

//停止UDP服务端监听任务
int Entity_Udp_Server_Task_Stop(Udp_Server_Task_Info_t *info);






