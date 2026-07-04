//entity_soc_mcu_protocol.c
#include "entity_soc_mcu_protocol.h"



#include "entity_log.h"
#include "com_utils.h"
#include "com_crc.h"

#include <string.h>

static Soc_Mcu_Cmd_Cbs_t Soc_Mcu_Cmd_Cbs;


/**
*@名称 		Soc_Mcu_Cmd_Cbs_Init
*@功能 		协议回调初始化设置
*@参数 		Soc_Mcu_Cmd_Cbs_t *cbs	
*@返回值 	void
*@使用说明  由应用层来对接
*/
void Soc_Mcu_Cmd_Cbs_Init(Soc_Mcu_Cmd_Cbs_t *cbs)
{
	Soc_Mcu_Cmd_Cbs = *cbs;
}


/**
*@名称 		Get_Soc_Mcu_Protocol_Msg_Total_Len
*@功能 		根据SOC与MCU协议得到消息总长度
*@参数 		unsigned char cmd, char *pdata, int len		
*@返回值 	void
*@使用说明  SDIO消息没有长度信息，只能通过此方式获取
*/
unsigned short Get_Soc_Mcu_Protocol_Msg_Total_Len(unsigned char *msg)
{
    unsigned short total_len = LE_BYTES_TO_INT16(&msg[1]);
    return total_len;
}


/**
*@名称 		Soc_Mcu_Protocol_Report
*@功能 		SOC与MCU协议上报
*@参数 		unsigned char cmd, char *pdata, int len		
*@返回值 	void
*@使用说明
*/
void Soc_Mcu_Protocol_Report(unsigned char cmd, unsigned char *pdata, int len)
{
	int offset = 0;
	unsigned char data_pack[MCU_SEND_MAX_SIZE];
    unsigned short total_len = 0;
    unsigned char xor_check = 0;

    total_len = len + SOC_MCU_MSG_PROTOCOL_DATA_SIZE;//在应答内容前加上执行结果

    data_pack[offset++] = SOC_MCU_MSG_HEAD; //帧头 
	INT16_TO_LE_BYTES(total_len, &data_pack[offset]);//数据总长度，小端序
    offset += 2;
    data_pack[offset++] = cmd; //命令字
	memcpy(&data_pack[offset], pdata, len);//返回数据
	offset += len;
    xor_check = Xor_Inverted_Check(&data_pack[0], offset);//异或校验
    data_pack[offset++] = xor_check;
	if(Soc_Mcu_Cmd_Cbs.Data_Send_Callback)
	{
		Soc_Mcu_Cmd_Cbs.Data_Send_Callback(data_pack, offset);
		Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_INFO, "report soc:", data_pack, offset);
	}
}

/**
*@名称 		Soc_Mcu_Protocol_Response
*@功能 		SOC与MCU协议应答
*@参数 		unsigned char cmd, unsigned char error_code,char *pdata, int len		
*@返回值 	void
*@使用说明
*/
void Soc_Mcu_Protocol_Response(unsigned char cmd, unsigned char error_code, unsigned char *pdata, int len)
{
    int offset = 0;
	unsigned char data_pack[MCU_SEND_MAX_SIZE];
    unsigned short total_len = 0;
    unsigned char xor_check = 0;

    total_len = len + 1 + SOC_MCU_MSG_PROTOCOL_DATA_SIZE;//在应答内容前加上执行结果

    data_pack[offset++] = SOC_MCU_MSG_HEAD; //帧头 
	INT16_TO_LE_BYTES(total_len, &data_pack[offset]);//数据总长度，小端序
    offset += 2;
    data_pack[offset++] = cmd; //命令字
	data_pack[offset++] = error_code;//执行结果
	memcpy(&data_pack[offset], pdata, len);//返回数据
	offset += len;
    xor_check = Xor_Inverted_Check(&data_pack[0], offset);//异或校验
    data_pack[offset++] = xor_check;
	if(Soc_Mcu_Cmd_Cbs.Data_Send_Callback)
	{
		Soc_Mcu_Cmd_Cbs.Data_Send_Callback(data_pack, offset);
	}
	if(cmd == SOC_MCU_CMD_QUICK_START_INFO)
	{
		if(pdata && len>0)
			ENTITY_LOGI("MCU--->SOC:quick info:%s\r\n", pdata);
		else
			ENTITY_LOGI("MCU--->SOC:no vaild quick info\r\n");
	}	
	else
	{
		Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_INFO, "MCU--->SOC:", data_pack, offset);
	}
}


/**
*@名称 		Soc_Protocol_Process
*@功能 		对从SOC接收到的数据根据命令进行解析
*@参数 		unsigned char cmd, unsigned char *pdata, unsigned short data_len		
*@返回值 	void
*@使用说明
*/
int Soc_Protocol_Process(unsigned char cmd, unsigned char *pdata, unsigned short data_len)
{
	unsigned char *result=NULL;
	unsigned short result_len=0;
	int ret=0;
	switch (cmd)
    {
        case SOC_MCU_CMD_START_KEEPALIVE:// 发送保活信息,并启动保活
		{
			ENTITY_LOGI("SOC----->MCU, soc cmd start keepalive\r\n");
			if(Soc_Mcu_Cmd_Cbs.Keepalive_Info_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Keepalive_Info_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_SLEEP:// 进入休眠
		{
			if(Soc_Mcu_Cmd_Cbs.Enter_Sleep_Callback)
			{
				Soc_Mcu_Cmd_Cbs.Enter_Sleep_Callback(pdata, data_len, &result, &result_len);
			}
			break;
		}
        case SOC_MCU_CMD_QUICK_START_INFO:// 获取快启信息, TCP唤醒
		{
			if(Soc_Mcu_Cmd_Cbs.Get_Quick_Start_Info_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Get_Quick_Start_Info_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_READY:// 上电数据通信同步,对方应答相关信息
		{
			if(Soc_Mcu_Cmd_Cbs.Ready_Sync_Data_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Ready_Sync_Data_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_SET_TIME:// 设置MCU时间
		{
			unsigned int stamp = LE_BYTES_TO_INT32(pdata);
			ENTITY_LOGI("SOC----->MCU, soc cmd set time:%d\r\n", stamp);
			if(Soc_Mcu_Cmd_Cbs.Set_Mcu_Time_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Set_Mcu_Time_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_STOP_KEEPALIVE://停止保活
		{
			ENTITY_LOGI("SOC----->MCU, soc cmd stop keepalive\r\n");
			if(Soc_Mcu_Cmd_Cbs.Stop_Keepalive_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Stop_Keepalive_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
       
        case SOC_MCU_CMD_GET_BAT_INFO://获取电量信息  充电状态+百分比
		{
			if(Soc_Mcu_Cmd_Cbs.Get_Battery_Info_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Get_Battery_Info_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_SET_LOW_BAT_THRESHOLD:// 设置电池低电量预警阈值
		{
			ENTITY_LOGI("SOC----->MCU, soc cmd set low bat threshold:%d\r\n", pdata[0]);
			if(Soc_Mcu_Cmd_Cbs.Set_Bat_Lower_Threshold_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Set_Bat_Lower_Threshold_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_SET_PIR_SWITCH:// 设置pir开关功能
		{
			ENTITY_LOGI("SOC----->MCU, soc cmd set pir switch:%d\r\n", pdata[0]);
			if(Soc_Mcu_Cmd_Cbs.Set_Pir_Switch_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Set_Pir_Switch_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_SET_PIR_INTERVAL:// 设置pir唤醒最小间隔
		{
			ENTITY_LOGI("SOC----->MCU, soc cmd set pir interval:%d\r\n", pdata[0]);
			if(Soc_Mcu_Cmd_Cbs.Set_Pir_Interval_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Set_Pir_Interval_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_SET_PIR_SENS_LEVEL://设置PIR灵敏度档位
		{
			if(Soc_Mcu_Cmd_Cbs.Set_Pir_Sens_Level_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Set_Pir_Sens_Level_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_SET_PIR_SENS_VALUE://设置PIR灵敏度当前档位阈值，用于测试
		{
			if(Soc_Mcu_Cmd_Cbs.Set_Pir_Sens_Value_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Set_Pir_Sens_Value_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_GET_PIR_SENS_VALUE://获取PIR灵敏度当前档位阈值，用于测试
		{	
			if(Soc_Mcu_Cmd_Cbs.Get_Pir_Sens_Value_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Get_Pir_Sens_Value_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_SET_LED_SWITCH:
		{
			if(Soc_Mcu_Cmd_Cbs.Set_Led_Switch_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Set_Led_Switch_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}

        case SOC_MCU_CMD_DEEP_SLEEP://通知MCU进入深度睡眠
		{
			ENTITY_LOGI("SOC----->MCU, soc cmd enter stop mode, only key can wakeup\r\n");
			if(Soc_Mcu_Cmd_Cbs.Enter_Deep_Sleep_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Enter_Deep_Sleep_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
        case SOC_MCU_CMD_HEARTBEAT://心跳
		{
			if(Soc_Mcu_Cmd_Cbs.Heartbeat_Check_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Heartbeat_Check_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_STOP_HEARTBEAT://停止心跳检测 
		{
			if(Soc_Mcu_Cmd_Cbs.Stop_Heartbeat_Check_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Stop_Heartbeat_Check_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}

//-----------------------------------------4G 低功耗扩展------------------------------------------------------
		case SOC_MCU_CMD_GET_TIME:
		{
			if(Soc_Mcu_Cmd_Cbs.Get_Mcu_Time_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Get_Mcu_Time_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_GET_MULTI_INFO:
		{
			if(Soc_Mcu_Cmd_Cbs.Get_Multi_Info_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Get_Multi_Info_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_RESET:// MCU重启
		{
			if(Soc_Mcu_Cmd_Cbs.Mcu_Reset_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Mcu_Reset_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_RESET_MCU_PARAM://MCU端保存到FLASH中的参数恢复默认值，SOC复位重置时需要调用此命令
		{
			if(Soc_Mcu_Cmd_Cbs.Mcu_Param_Reset_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Mcu_Param_Reset_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_CAT1_INIT:// 命令CAT1重新初始化
		{
			if(Soc_Mcu_Cmd_Cbs.Cat1_Reinit_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Cat1_Reinit_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_GET_CAT1_STATE://获取CAT1模组当前状态
		{
			if(Soc_Mcu_Cmd_Cbs.Get_Cat1_State_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Get_Cat1_State_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_EXT_UART_SET://mcu扩展串口数据交互
		{
			if(Soc_Mcu_Cmd_Cbs.Ext_Uart_Data_Send_Callback)
			{
				ret = Soc_Mcu_Cmd_Cbs.Ext_Uart_Data_Send_Callback(pdata, data_len, &result, &result_len);
				Soc_Mcu_Protocol_Response(cmd, ret, result, result_len);
			}
			break;
		}
		case SOC_MCU_CMD_OTA_START://启动OTA升级
		{
			ENTITY_LOGI("SOC----->MCU, ota start\r\n");
			//Mcu_Ota_Start_Process(pdata, data_len);
			break;
		}
		case SOC_MCU_CMD_OTA_SEND_DATA://传输OTA数据
		{
			ENTITY_LOGI("SOC----->MCU, ota send data\r\n");
			//Mcu_Ota_Send_Data_Process(pdata, data_len);
			break;
		}

		case SOC_MCU_CMD_OTA_COMPLETE://结束OTA升级
		{
			ENTITY_LOGI("SOC----->MCU, ota complete\r\n");
			//Mcu_Ota_Complete_Process(pdata, data_len);
			break;
		}
		
		default:
			break;
	}
	return 0;
}


/**
*@名称 		Soc_Mcu_Protocol_Check
*@功能 		SOC与MCU协议格式检查 
*@参数 		unsigned char *pdata, unsigned short data_len		
*@返回值 	void
*@使用说明
*/
int Soc_Mcu_Protocol_Check(unsigned char *pdata, unsigned short data_len)
{
	unsigned short offset=0;
	
	unsigned char head = pdata[offset++];
	if(head != SOC_MCU_MSG_HEAD)
	{
		ENTITY_LOGD("%s, head error\r\n", __FUNCTION__);
		return SOC_MCU_PROTOCOL_ERROR_HEAD;//协议头部错误
	}
	unsigned short msg_len = LE_BYTES_TO_INT16(&pdata[offset]);//小端序提取
	offset += 2;
	ENTITY_LOGD("msg_len:%d\r\n", msg_len);
	if(msg_len > MCU_REC_MAX_SIZE || msg_len > data_len)
	{
		ENTITY_LOGD("%s, len error\r\n", __FUNCTION__);
		return SOC_MCU_PROTOCOL_ERROR_LEN;//长度错误
	}
	unsigned char xor_value = pdata[msg_len-1];
	unsigned char check_value = Xor_Inverted_Check(pdata, msg_len-1);
	ENTITY_LOGD("xor_value:%d, check_value:%d\r\n", xor_value, check_value);
	if(xor_value != check_value)
	{
		ENTITY_LOGD("%s, crc check error\r\n", __FUNCTION__);
		return SOC_MCU_PROTOCOL_ERROR_CRC;//校验错误
	}
	return SOC_MCU_PROTOCOL_ERROR_NONE;
}


/**
*@名称 		Rec_Data_From_Soc_Parse
*@功能 		对从SOC接收到的数据进行协议解析
*@参数 		unsigned char *msg, unsigned short msg_len		
*@返回值 	void
*@使用说明
*/
int Rec_Data_From_Soc_Parse(unsigned char *msg, unsigned short msg_len)
{
	unsigned char ret;
    unsigned short vaild_msg_len = msg_len;
    if(msg_len == 0)
	{
		vaild_msg_len = Get_Soc_Mcu_Protocol_Msg_Total_Len(msg);
	}

	while(vaild_msg_len >= SOC_MCU_MSG_PROTOCOL_DATA_SIZE)
	{
		ret = Soc_Mcu_Protocol_Check(msg, vaild_msg_len);
		if(ret != SOC_MCU_PROTOCOL_ERROR_NONE)
		{
			msg++;
			vaild_msg_len--;
		}
		else
		{
			unsigned short total_len = (unsigned short)msg[1] | msg[2] << 8;//小端序提取
			unsigned short data_len = total_len-SOC_MCU_MSG_PROTOCOL_DATA_SIZE;
			unsigned char cmd = msg[3];
			unsigned char *pdata = &msg[4];
			
			if(cmd == SOC_MCU_CMD_OTA_SEND_DATA)
				Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_INFO, "\r\nrec soc msg:", msg, 10);
			else
				Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_INFO, "\r\nrec soc msg:", msg, total_len);
			
			Soc_Protocol_Process(cmd, pdata, data_len);
			msg += total_len;
			vaild_msg_len -= total_len;
		}
			
	}
    return 0;
}

























