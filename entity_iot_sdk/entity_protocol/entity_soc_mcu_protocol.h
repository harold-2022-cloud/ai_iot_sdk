//entity_soc_mcu_protocol.h
#pragma once


#include "com_utils.h"

/*
SOC与MCU通信协议

字节序：小端序
帧头：固定为0XAB		
数据长度:包含从帧头到校验的所有数据总长度
异或校验：在此之前所有消息数据的异或校验

请求：
Header  Length 		Cmd			Data 		XOR Checksum
帧头	数据长度		命令字		数据			异或校验
1字节	2字节		1字节		n字节		1字节

应答：
Header  Length 		Cmd			Data 		XOR Checksum
帧头	数据长度		命令字		负载数据			异或校验
1字节	2字节		1字节		n字节		1字节
负载数据包含：结果(1字节)+具体数据
结果：0-成功 非0为错误码
如果应答错误则无后续具体数据

//错误码
0-无错误
1-协议头部错误
2-校验错误
3-命令字错误
4-长度错误
5-执行失败
*/

typedef enum
{
	SOC_MCU_PROTOCOL_ERROR_NONE=0,	//无错误
	SOC_MCU_PROTOCOL_ERROR_HEAD=1,	//协议头部错误
	SOC_MCU_PROTOCOL_ERROR_CRC=2,		//校验错误
	SOC_MCU_PROTOCOL_ERROR_CMD=3,		//命令字错误
	SOC_MCU_PROTOCOL_ERROR_LEN=4,		//长度错误
	SOC_MCU_PROTOCOL_ERROR_EXEC_FAIL=5,	//执行失败
}Soc_Mcu_Protocol_Error_e;


#define SOC_MCU_MSG_HEAD 			0xAB	//通信协议头部
#define SOC_MCU_MSG_HEAD_SIZE 		1		//头数1字节
#define SOC_MCU_MSG_LEN_SIZE 		2
#define SOC_MCU_MSG_CMD_SIZE 		1
#define SOC_MCU_MSG_CRC_SIZE 		1

#define MCU_SEND_MAX_SIZE			2048		//MCU发送消息最大长度
#define MCU_REC_MAX_SIZE			(512+32)	//MCU接收消息最大长度

#define SOC_MCU_MSG_PROTOCOL_DATA_SIZE		5//除了数据外的协议数据长度，包含头部，长度，命令，校验

#define SOC_MCU_MSG_SOC_DATA_SIZE                                                                                      \
    (MCU_REC_MAX_SIZE - SOC_MCU_MSG_PROTOCOL_DATA_SIZE)



typedef enum
{
	MCU_EXT_UART_CMD_FORWARD=0,		//转发
	MCU_EXT_UART_CMD_SET_BAUDRATE,	//设置MCU与扩展串口通信波特率
}Mcu_Ext_Uart_Cmd_Type_e;


//soc与mcu通信命令字
typedef enum
{
	SOC_MCU_CMD_GET_RSSI = 0X90,						//获取CAT1模组信号强度dbm，4字节
	SOC_MCU_CMD_GET_TIME = 0X91,						//获取MCU时间戳 #
	SOC_MCU_CMD_GET_MULTI_INFO=0X92,					//获到PIR,电量，复位键等相关信息 #
	SOC_MCU_CMD_EXT_UART_SET=0X93,						//mcu扩展串口数据交互 #

	SOC_MCU_CMD_READY = 0xA0,                 			// 上电数据通信同步,对方应答相关信息 *
	SOC_MCU_CMD_SLEEP = 0xA1,                 			// 进入休眠		*
	SOC_MCU_CMD_RESET = 0xA2,                  			// MCU重启	#
	SOC_MCU_CMD_KEEPALIVE_INFO = 0xA3,        			// 发送保活信息 *
	SOC_MCU_CMD_QUICK_START_INFO = 0xA4,      			// 获取快启信息 *
	SOC_MCU_CMD_SET_TIME = 0xA5,              			// 设置MCU时间 *
	SOC_MCU_CMD_CLOSE_TCP = 0xA6,             			// 断开MCU TCP连接,停止发送保活数据 *
	SOC_MCU_CMD_SET_PIR_SWITCH = 0xA7,        			// 设置pir开关功能 *
	SOC_MCU_CMD_GET_BAT_INFO = 0xA8,          			// 获取电池信息	*
	SOC_MCU_CMD_SET_LOW_BAT_THRESHOLD = 0xA9, 			// 设置电池低电量预警阈值 *
	SOC_MCU_CMD_SET_PIR_INTERVAL = 0xAA,      			// 设置pir唤醒最小间隔 *
	SOC_MCU_CMD_SET_LED_SWITCH = 0xAB,        			// 设置LED状态开关, 充电，工作 状态指示灯的功能开关 *
	SOC_MCU_CMD_HEARTBEAT = 0xAC,             			// 发送soc与mcu心跳包 *
	SOC_MCU_CMD_STOP_HEARTBEAT = 0xAD,             		// 停止心跳包检测 *
	SOC_MCU_CMD_DEEP_SLEEP = 0xAE,            			// 通知MCU进入深度睡眠 *
	SOC_MCU_CMD_SET_PIR_SENS_LEVEL = 0xAF,				//设置PIR灵敏度档位 0-低  1-中  2-高 *

	//蓝牙相关


	//4G相关
	SOC_MCU_CMD_CAT1_INIT = 0xC0,      					// 命令CAT1重新初始化 #
	SOC_MCU_CMD_GET_CAT1_STATE = 0xC1,					//获取CAT1模组当前状态 #
	//SOC_MCU_CMD_SET_CAT1_AT_CHANNEL = 0xC2,					//设置CAT1模组AT指令激活场景通道1-N，EG-800G-EULD默认2
																											 // MCU to SOC

	//上报相关
	//ATBM_RN_MSG_MCU_TO_SOC_CLOSE_AP = 0xD0,              // 通知SOC关闭AP配网线程
	//ATBM_RN_MSG_MCU_TO_SOC_REPORT_BT_BIND_INFO = 0xD2,   // 上报蓝牙绑定信息
	SOC_MCU_CMD_REPORT_LOW_POWER = 0xD3,      			// 上报低电
	SOC_MCU_CMD_REPORT_RSSI = 0xD4,      				// 上报CAT1模组信号强度dbm，4字节

	//
	SOC_MCU_CMD_RESET_MCU_PARAM = 0xE0,      			//MCU端保存到FLASH中的参数恢复默认值，SOC复位重置时需要调用此命令 #
	SOC_MCU_CMD_SET_PIR_SENS_VALUE = 0xE1,      		// 设置PIR灵敏度当前档位阈值，用于测试 *
	SOC_MCU_CMD_GET_PIR_SENS_VALUE = 0xE2,      		// 获取PIR灵敏度当前档位阈值，用于测试 *

	//OTA升级相关
	SOC_MCU_CMD_OTA_START		=0XF0,	//启动升级，发送固件包大小，返回传输数据包大小 #
	SOC_MCU_CMD_OTA_SEND_DATA	=0XF1,	//传输数据，偏移量 OTA数据长度 OTA数据  返回OTA数据长度  #
	SOC_MCU_CMD_OTA_COMPLETE	=0XF2,	//结束升级 CRC32校验 #

	SOC_MCU_CMD_START_KEEPALIVE=SOC_MCU_CMD_KEEPALIVE_INFO,//启动保活
	SOC_MCU_CMD_STOP_KEEPALIVE=SOC_MCU_CMD_CLOSE_TCP,		//停止保活
}Soc_Mcu_Cmd_e;


//协议命令处理回调
typedef int (*Soc_Mcu_Cmd_Callback_f)(unsigned char *msg, unsigned short msg_len, unsigned char **result, unsigned short *result_len);

typedef struct 
{
	void (*Data_Send_Callback)(unsigned char *msg, unsigned short msg_len);//协议数据的发送回调

	//----------------------此区间是WIFI低功耗必须支持的协议----------------------//
	Soc_Mcu_Cmd_Callback_f Keepalive_Info_Callback;			//SOC发送保活信息
	Soc_Mcu_Cmd_Callback_f Enter_Sleep_Callback;			//SOC命令MCU进入休眠
	Soc_Mcu_Cmd_Callback_f Get_Quick_Start_Info_Callback;	//SOC获取快启信息
	Soc_Mcu_Cmd_Callback_f Ready_Sync_Data_Callback;		//就绪双方数据同步
	Soc_Mcu_Cmd_Callback_f Set_Mcu_Time_Callback;			//设置MCU时间
	Soc_Mcu_Cmd_Callback_f Stop_Keepalive_Callback;			//MCU停止TCP保活
	Soc_Mcu_Cmd_Callback_f Get_Battery_Info_Callback;		//获取电池信息
	Soc_Mcu_Cmd_Callback_f Set_Bat_Lower_Threshold_Callback;//设置电池低电量预警阈值
	Soc_Mcu_Cmd_Callback_f Set_Pir_Switch_Callback;			//设置pir开关功能
	Soc_Mcu_Cmd_Callback_f Set_Pir_Interval_Callback;		// 设置pir唤醒最小间隔
	Soc_Mcu_Cmd_Callback_f Set_Pir_Sens_Level_Callback;		//设置PIR灵敏度档位 0-低  1-中  2-高
	Soc_Mcu_Cmd_Callback_f Set_Pir_Sens_Value_Callback;		// 设置PIR灵敏度当前档位阈值
	Soc_Mcu_Cmd_Callback_f Get_Pir_Sens_Value_Callback;		// 获取PIR灵敏度当前档位阈值
	Soc_Mcu_Cmd_Callback_f Set_Led_Switch_Callback;			// 设置LED状态开关
	Soc_Mcu_Cmd_Callback_f Enter_Deep_Sleep_Callback;		// 通知MCU进入深度睡眠
	Soc_Mcu_Cmd_Callback_f Heartbeat_Check_Callback;		//启动及发送SOC与MCU间心跳包
	Soc_Mcu_Cmd_Callback_f Stop_Heartbeat_Check_Callback;	// 停止心跳包检测
	//--------------------------------WIFI END----------------------------------//

	//-----------------------此段为4G低功耗扩展的协议-----------------------------//
	Soc_Mcu_Cmd_Callback_f Get_Mcu_Time_Callback;			//获取MCU时间戳
	Soc_Mcu_Cmd_Callback_f Get_Multi_Info_Callback;			//获到PIR,电量，复位键等相关信息
	Soc_Mcu_Cmd_Callback_f Mcu_Reset_Callback;				//MCU重启
	Soc_Mcu_Cmd_Callback_f Mcu_Param_Reset_Callback;		//MCU端保存到FLASH中的参数恢复默认值，SOC复位重置时需要调用此命令
	Soc_Mcu_Cmd_Callback_f Cat1_Reinit_Callback;			//命令CAT1重新初始化
	Soc_Mcu_Cmd_Callback_f Get_Cat1_State_Callback;			//获取CAT1模组当前状态
	Soc_Mcu_Cmd_Callback_f Ext_Uart_Data_Send_Callback;		//mcu扩展串口数据交互
	Soc_Mcu_Cmd_Callback_f Ota_Start_Callback;				//启动升级，发送固件包大小，返回传输数据包大小
	Soc_Mcu_Cmd_Callback_f Ota_Send_Data_Callback;			//传输数据，偏移量 OTA数据长度 OTA数据  返回OTA数据长度
	Soc_Mcu_Cmd_Callback_f Ota_Complete_Callback;			//结束升级 CRC32校验


}Soc_Mcu_Cmd_Cbs_t;


typedef struct
{
    unsigned char Flag_Charging; // 是否在充电
    unsigned char Battery_Percent; // 电池电量百分比
} __attribute__((packed)) Respone_Bat_Info_t;

typedef struct
{
    char Mcu_Version[8];         // MCU版本, 字符串
    unsigned int Stamp;         // 时间戳
    unsigned char Wake_Source; 	// 唤醒源
	Respone_Bat_Info_t Bat_Info;		//电量信息
}__attribute__((packed)) Respone_Lowpower_Info_t;//取消编译器默认的字节对齐方式,不对齐


#define OTA_PACKET_SIZE		512
typedef struct
{
	unsigned int Bin_Size;	//文件总大小
	unsigned int Crc32_Value;//校验
	unsigned int Offset;	//累积的偏移量
	unsigned int Packet_Size;//传输数据包大小
	unsigned int Saved_Crc32;//累积的CRC32值
}Mcu_Ota_Info_t;

/*
字节序为：小端序
OTA启动传输
	SOC请求：
		ab 09 00 f0 00 68 00 00 3a		//固件包长度(4字节) 0X00006800=26624，即 26KB, ，小端序
	MCU应答：
		ab 08 00 f0 00 00 02 51		//应答成功及数据包大小(2字节) 0X0200=512 ，小端序
		ab 06 00 f0 05 58			//应答执行失败
OTA数据传输
	SOC请求：
		ab 0b 02 f1 00 00 00 00 00 02 ... 3a		//偏移量(4字节)+数据长度(2字节)+数据
	MCU应答：
		ab 08 00 f1 00 00 02 50					//返回成功，数据长度(2字节)
		ab 06 00 f1 05 59					//返回失败

OTA结束传输
	SOC请求：
		ab 09 00 f2 XX XX XX XX YY		//固件总CRC32校验值(4字节) 
	MCU应答：
		ab 06 00 f2 00 5f			//返回升级结果，0-成功 
		ab 06 00 f2 05 5a			//返回升级结果，5-执行失败
*/


//根据SOC与MCU协议得到消息总长度
unsigned short Get_Soc_Mcu_Protocol_Msg_Total_Len(unsigned char *msg);

//OC与MCU协议上报
void Soc_Mcu_Protocol_Report(unsigned char cmd, unsigned char *pdata, int len);

//对从SOC接收到的数据进行协议解析
int Rec_Data_From_Soc_Parse(unsigned char *msg, unsigned short msg_len);


void Soc_Mcu_Cmd_Cbs_Init(Soc_Mcu_Cmd_Cbs_t *cbs);






































