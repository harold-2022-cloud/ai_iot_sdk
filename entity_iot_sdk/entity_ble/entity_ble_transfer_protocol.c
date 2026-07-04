//entity_ble_transfer_protocol.c
#include "entity_ble_transfer_protocol.h"

#include "entity_ble_gatt.h"

#include <string.h>
#include <sys/time.h>
#include <inttypes.h>

#include "cJSON.h"
#include "entity_log.h"
#include "entity_iot_func.h"
#include "com_utils.h"
#include "com_crc.h"

static Ble_Msg_App_Process_f Entity_Ble_Msg_App_Callback;

static Entity_Ble_Gatt_Packet_Info_t Entity_Ble_Gatt_Packet_Info;



/**
*@名称 		Entity_Ble_V1_Register_Msg_App_Process_Cb
*@功能 		注册蓝牙数据应用层处理回调
*@参数 		Ble_Msg_App_Process_f callback
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_V1_Register_Msg_App_Process_Cb(Ble_Msg_App_Process_f callback)
{
    Entity_Ble_Msg_App_Callback = callback;
}


/**
*@名称 		Entity_Ble_V1_Make_Frame
*@功能 		根据蓝牙V1协议将需要发送的数据进行组装
*@参数 		unsigned short *outlen,   加上包头数据后实际总的数据长度
*@参数 		unsigned char type,       传输的数据类型
*@参数 		const void *data,   要传输的数据
*@参数 		unsigned short len        要传输的数据长度
*@返回值 	unsigned char*            申请的空间地址（所有数据都已填充好）
*@使用说明	
*/
unsigned char* Entity_Ble_V1_Make_Frame(unsigned short *outlen, unsigned char type, const void *data, unsigned short len)
{
    if (data == NULL)
        return NULL;
    unsigned short offset=0;
    unsigned short total_pack_num = len / PER_DATA_LEN;//按最大有效数据量118为包，需要发送的总包数
    unsigned short remain = len % PER_DATA_LEN;//不完整包的数据量
    if(remain)
        total_pack_num++;
   
    *outlen = PROTOCOL_DATA_LEN * total_pack_num + len;//加上协议数据后的总长度
    ENTITY_LOGI("inlen:%d, outlen:%d\r\n", len, *outlen);
    unsigned char *out = (unsigned char *)Entity_Mem_Malloc(*outlen);
    if (out == NULL)
    {
        ENTITY_LOGE("%s, malloc Failed!", __FUNCTION__);
        return NULL;
    }
        
    unsigned short cur_data_len = PER_DATA_LEN;//一包最大加载有效数据长度

    for(int i=0; i<total_pack_num; i++)
    {
        if(remain && i==total_pack_num-1)//最后一包不满   
            cur_data_len = remain;
        out[offset++] = PROTOCOL_HEAD_V1;
        out[offset++] = type;
        out[offset++] = i >> 8;             //包序号， 从0开始
        out[offset++] = i;
        out[offset++] = total_pack_num >> 8;//总包数
        out[offset++] = total_pack_num;
        out[offset++] = len >> 8;           //有效数据总长度
        out[offset++] = len;
        out[offset++] = cur_data_len;       //当前包有效数据长度

        memcpy(&out[offset], data+PER_DATA_LEN*i, cur_data_len);
        offset += cur_data_len;
        out[offset] = Check_Sum(out+offset+2-PROTOCOL_DATA_LEN-cur_data_len, cur_data_len+PROTOCOL_DATA_LEN-2);//第一个数据不参与校验
        offset++;
    }
    return out;
}

/**
*@名称 		Entity_Ble_V1_Send_Packet_By_Notify
*@功能 		根据蓝牙V1协议向APP端发送数据
*@参数 		unsigned char *send_data, unsigned short len
*@返回值 	int      0-成功      
*@使用说明	
*/
int Entity_Ble_V1_Send_Packet_By_Notify(unsigned char *send_data, unsigned short len)
{
    unsigned short datalen = 0;
    unsigned char *data = Entity_Ble_V1_Make_Frame(&datalen, PROTOCOL_DATA_TYPE, send_data, len);
    if(data == NULL)
        return -1;
    //ENTITY_LOGD("send data:[%s], len:[%d]", send_data, len);
    //Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_DEBUG, "send frame data:", data, datalen);
    
    unsigned short total_pack_num = datalen / PER_PACKAGE_LEN;//总包数
    unsigned short remain = datalen % PER_PACKAGE_LEN;//不完整包的数据量
    if(remain)
        total_pack_num++;
    unsigned short cur_data_len = PER_PACKAGE_LEN;//一包最大数据长度
    for(int i=0; i<total_pack_num; i++)
    {
        if(remain && i==total_pack_num-1)//最后一包不满   
            cur_data_len = remain;
        if(Entity_Ble_Gatt_Notify_Send(data+i*PER_PACKAGE_LEN, cur_data_len) != 0)
        {
            ENTITY_LOGE("%s, Bsp_Ble_Gatt_Indicate_Send Failed!", __FUNCTION__);
            Entity_Mem_Free(data);
            return -1;
        }
        Entity_Sleep_Ms(20);
    }
    Entity_Mem_Free(data);
    ENTITY_LOGD("send notify total_pack_num:%d, remain:%d\r\n", total_pack_num, remain);
    return 0;
}

/**
*@名称 		Entity_Ble_V1_Recv_Response
*@功能 		根据蓝牙V1协议向APP端应答
*@参数 		unsigned char cmd_type, int code, const char *msg
*@返回值 	int      0-成功      
*@使用说明	
*/
void Entity_Ble_V1_Recv_Response(unsigned char cmd_type, int code, const char *msg)
{   
    cJSON *obj = cJSON_CreateObject();
    if (!obj)
    {
        ENTITY_LOGE("cJSON_CreateObject err\r\n");
        return;
    }
    ENTITY_LOGD("cJSON_CreateObject success\r\n");
    
    unsigned int timestamp = Entity_Get_Time_Stamp();
    ENTITY_LOGI("%s, timestamp:%d, code:%d\r\n", __FUNCTION__, timestamp, code);
    cJSON_AddNumberToObject(obj, "ts", timestamp);
    cJSON_AddNumberToObject(obj, "code", code);
    cJSON_AddStringToObject(obj, "msg", msg);
    cJSON_AddNumberToObject(obj, "cmd_type", cmd_type);
    char *out = cJSON_PrintUnformatted(obj);
    if (!out)
    {
        ENTITY_LOGE("cJSON_PrintUnformatted err");
        return;
    }
    ENTITY_LOGI("cJSON_PrintUnformatted success");
    cJSON_Delete(obj);

    Entity_Ble_V1_Send_Packet_By_Notify((unsigned char*)out, strlen(out));
    if (out)
        Entity_Mem_Free(out);
}


/**
*@名称 		Entity_Ble_V1_Parser_Frame
*@功能 		根据蓝牙V1协议解析收到APP端的一帧数据
*@参数 		unsigned char *out,   本帧有效数据存放区
*@参数 		unsigned char *data,  收到的数据
*@参数 		unsigned short len    收到的数据长度
*@返回值 	int             消息解包成功与否 0：成功  非0：失败
*@使用说明	数据分包接收处理
*/
int Entity_Ble_V1_Parser_Frame(unsigned char *out, unsigned char *data, unsigned short len)
{
    unsigned char cmd_type = data[1];
    if (PROTOCOL_HEAD_V1 != data[0])//数据头固定为0XFF
    {
        ENTITY_LOGE("@-->head err!");
        Entity_Ble_V1_Recv_Response(cmd_type, ENTITY_BLE_DATA_PARSER_HEAD_ERR, "head err");
        return -1;
    }
    
    Entity_Ble_Gatt_Packet_Info.Current_Sn = (data[2] << 8) | data[3];//当前包序号，大端

    if (Entity_Ble_Gatt_Packet_Info.Current_Sn == 0)//第一包，参数复位
    {
        Entity_Ble_Gatt_Packet_Info.Recv_Sn = 1; //已接收包数清零
        Entity_Ble_Gatt_Packet_Info.Recv_Len = 0;//已接收数据长度
        ENTITY_LOGI("@---------->Ble First Packages\r\n");
    }
    else
    {
        ++Entity_Ble_Gatt_Packet_Info.Recv_Sn;
        if (Entity_Ble_Gatt_Packet_Info.Current_Sn+1 != Entity_Ble_Gatt_Packet_Info.Recv_Sn)//包序号不是按顺序接收
        {
            ENTITY_LOGE("@-->sn err! csn:[%d] lsn:[%d]", Entity_Ble_Gatt_Packet_Info.Current_Sn, Entity_Ble_Gatt_Packet_Info.Recv_Sn);
            Entity_Ble_V1_Recv_Response(cmd_type, ENTITY_BLE_DATA_PARSER_SN_ERR, "sn err");
            return -2;
        }
    }

    unsigned char crc = Check_Sum(data + 1, len - 2);
    ENTITY_LOGI("crc:[%02x] [%02x]", crc, data[len - 1]);

    if (crc != data[len - 1])
    {
        Entity_Ble_V1_Recv_Response(cmd_type, ENTITY_BLE_DATA_PARSER_CRC_ERR, "crc err");
        return -3;
    }
    Entity_Ble_Gatt_Packet_Info.Cmd_Type = cmd_type;//数据类型
    Entity_Ble_Gatt_Packet_Info.Total_Sn = (data[4] << 8) | data[5];//总包数
    Entity_Ble_Gatt_Packet_Info.Total_Len = (data[6] << 8) | data[7];//数据总长度

    
    if (0 == Entity_Ble_Gatt_Packet_Info.Current_Sn)//第一包需要申请内存
    {
        if(Entity_Ble_Gatt_Packet_Info.Buf)
            Entity_Mem_Free(Entity_Ble_Gatt_Packet_Info.Buf);
        Entity_Ble_Gatt_Packet_Info.Buf = Entity_Mem_Malloc(Entity_Ble_Gatt_Packet_Info.Total_Len + 1);
        if (Entity_Ble_Gatt_Packet_Info.Buf == NULL)
        {
            ENTITY_LOGE("malloc failed\r\n");
            Entity_Ble_V1_Recv_Response(Entity_Ble_Gatt_Packet_Info.Cmd_Type, ENTITY_BLE_DATA_PARSER_MEM_1_ERR, "mem 1 err");
            return -4;
        }
        memset(Entity_Ble_Gatt_Packet_Info.Buf, 0, (Entity_Ble_Gatt_Packet_Info.Total_Len + 1));
    }
    unsigned char current_len = data[8];
    memcpy(out, data+9, current_len);
    return current_len;
}

/**
*@名称 		Entity_Ble_V1_Parser_Data
*@功能 		根据蓝牙V1协议解析收到数据
*@参数 		unsigned char *data,  收到的数据
*@参数 		unsigned short len    收到的数据长度
*@返回值 	int             消息解包成功与否 0：成功  非0：失败
*@使用说明	
*/
int Entity_Ble_V1_Parser_Data(unsigned char* data, unsigned short len)
{
    int ret = 0;
    unsigned char out[PER_PACKAGE_LEN] = {0};

    ret = Entity_Ble_V1_Parser_Frame(out, data, len);//按照蓝牙协议解析数据
    if (ret < 0)
    {
        return ret;
    }

    memcpy(Entity_Ble_Gatt_Packet_Info.Buf+Entity_Ble_Gatt_Packet_Info.Recv_Len, out, ret);
    Entity_Ble_Gatt_Packet_Info.Recv_Len += ret;
    ENTITY_LOGI("@-->current_sn:[%d] total_sn:[%d] Recv_Len:[%d], total_len:[%d]", Entity_Ble_Gatt_Packet_Info.Current_Sn, Entity_Ble_Gatt_Packet_Info.Total_Sn,\
                                    Entity_Ble_Gatt_Packet_Info.Recv_Len, Entity_Ble_Gatt_Packet_Info.Total_Len);
    if (Entity_Ble_Gatt_Packet_Info.Current_Sn+1 == (Entity_Ble_Gatt_Packet_Info.Total_Sn))//最后一包
    {
        if (Entity_Ble_Gatt_Packet_Info.Recv_Len == Entity_Ble_Gatt_Packet_Info.Total_Len)//数据已收齐
        {
            ENTITY_LOGI("rec success\r\n");
            Entity_Ble_V1_Recv_Response( Entity_Ble_Gatt_Packet_Info.Cmd_Type, 0, "ble recv all data success");
            if(Entity_Ble_Msg_App_Callback)
                Entity_Ble_Msg_App_Callback(Entity_Ble_Gatt_Packet_Info.Buf, Entity_Ble_Gatt_Packet_Info.Total_Len);
            Entity_Mem_Free(Entity_Ble_Gatt_Packet_Info.Buf);
            Entity_Ble_Gatt_Packet_Info.Buf = NULL;
        }
        else
        {
            ENTITY_LOGE("@-->ble data len err\r\n");
            Entity_Ble_V1_Recv_Response(Entity_Ble_Gatt_Packet_Info.Cmd_Type, ENTITY_BLE_DATA_PARSER_LEN_ERR, "len err");
            return -100;
        }
    }
    return ret;
}
