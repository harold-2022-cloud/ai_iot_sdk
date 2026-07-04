//entity_tcp_mbedtls.c
#include "entity_tcp_mbedtls.h"

#include "com_mbedtls.h"
#include "com_utils.h"
#include "entity_log.h"
#include "entity_iot_func.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define NEED_CONSERVE_RAM   //需要以节约内存的方式


/**
*@名称 		Entity_Get_Signature
*@功能 		获取加密后的签名数据
*@参数 		char *random, char *uuid, char *secret, uint32_t current_time, unsigned char *signature, size_t*sign_data_len
*@返回值 	int
*@使用说明	
*/
int Entity_Get_Signature(char *random, char *uuid, char *secret, uint32_t current_time, unsigned char *signature, size_t *sig_base64_len)
{
	char str[128] = {0};
    sprintf(str, "%s:%d:%s", uuid, (int)current_time, random);//uuid:time:random拼接
	unsigned char digest[SHA1_DIGEST_SIZE];
    int digest_length = Hmac_Sha1(str, secret, digest);//method:0
    mbedtls_base64_encode(signature, 64, sig_base64_len, digest, digest_length);
	return 0;
}

/**
*@名称 		Entity_Get_Encryption_Data
*@功能 		获取加密数据
*@参数 		uint8_t type, char *costom_str, char *uuid, char *secret, char *aes_key, unsigned char *encrypt_data, int *encrypt_data_len
*@返回值 	int
*@使用说明	
*/
int Entity_Get_Encryption_Data(uint8_t type, char *costom_str, char *uuid, char *secret, char *aes_key, unsigned char *encrypt_data, int *encrypt_data_len)
{
    char secret_mask[32];
    char aes_key_mask[32];
    Utils_Mask_Secret(secret, secret_mask, sizeof(secret_mask));
    Utils_Mask_Secret(aes_key, aes_key_mask, sizeof(aes_key_mask));
	ENTITY_LOGI("UUID = %s, SecretMask = %s, aes_key_mask = %s\r\n", uuid, secret_mask, aes_key_mask);

	//1、自定义随机16字节数据，base64
    char iv[16];
    Get_Random_Str(iv, 16);//最后一字节为\0
    char iv_base64[32];
    size_t iv_base64_len;
    mbedtls_base64_encode((unsigned char *)iv_base64, sizeof(iv_base64), &iv_base64_len, (unsigned char *)iv, 16);
	//2、加密后的三元组uuid，加密规则：对uuid字节数组使用原始iv字节数组作为向量，固定key进行aes cbc 128 加密方式，
	//采用 PKCS7Padding 方式填充，base64后传输
	char uid[64];
    memset(uid, 0, sizeof(uid));
    memcpy(uid, uuid, strlen(uuid));
    int uid_ret = Pkcs7_Padding(uid, sizeof(uid), strlen(uid));
    unsigned char uid_enc[uid_ret];
    Aes_Encrypt((unsigned char *)uid, uid_ret, (unsigned char*)aes_key, iv, uid_enc);
    unsigned char uid_base64[64];
    size_t uid_base64_len;
    mbedtls_base64_encode(uid_base64, sizeof(uid_base64), &uid_base64_len, uid_enc, uid_ret);
	
	//3、data数据处理
	uint32_t current_time = Entity_Get_Time_Stamp();
    //ENTITY_LOGD("current_time:%d\n", current_time);
    char random[27] = {0};
    Get_Random_Str(random, 27);
	
	unsigned char signature[64];
	size_t sig_base64_len;
	char data[256] = {0};
	
	if(type == KEEPALIVE_CMD_TYPE_AUTH_REQUEST)//鉴权才需要签名
	{
		Entity_Get_Signature(random, uuid, secret, current_time, signature, &sig_base64_len);
		
		sprintf(data, "{\"method\":0,\"time\":\"%d\",\"random\":\"%s\",\"signature\":\"%s\"}", (int)current_time, random, signature);
	}
	else //心跳上报
	{
		if(costom_str)
		{
			ENTITY_LOGD("%s, costom_str:%s\r\n", __func__, costom_str);
			sprintf(data, "{%s,\"time\":\"%d\",\"random\":\"%s\"}", costom_str, (int)current_time, random);
		}
		else
			sprintf(data, "{\"time\":\"%d\",\"random\":\"%s\"}", (int)current_time, random);
	}
	ENTITY_LOGD("%s, data:%s\r\n", __func__, data);
	
	int data_ret = Pkcs7_Padding(data, sizeof(data), strlen(data));
	//ENTITY_LOGD("[%s]: pkcs7_padding, data_ret = %d \n", __func__, data_ret);
    unsigned char data_enc[data_ret];
    Aes_Encrypt((unsigned char *)data, data_ret, (unsigned char*)aes_key, iv, data_enc);
	//Dump_Hex_To_String(LOG_LEVEL_DEBUG,"data_enc hex:", data_enc, data_ret);
	//ENTITY_LOGD("\r\n data_enc str:%s\r\n", data_enc);

    unsigned char data_base64[256];
    size_t data_base64_len;
    mbedtls_base64_encode(data_base64, sizeof(data_base64), &data_base64_len, data_enc, data_ret);
	
	//加载数据到缓存区
	int pos = 0;
	if(type == KEEPALIVE_CMD_TYPE_AUTH_REQUEST)//鉴权
	{
		encrypt_data[pos++] = ENTITY_AUTH_VERSION;
		encrypt_data[pos++] = ENTITY_AUTH_TYPE;
		encrypt_data[pos++] = ENTITY_AUTH_FLAG;
	}
	else
	{
		encrypt_data[pos++] = ENTITY_HEARTBEAT_VERSION;
		encrypt_data[pos++] = ENTITY_HEARTBEAT_TYPE;
		encrypt_data[pos++] = ENTITY_HEARTBEAT_FLAG;
	}
    //payload_len
    int payload_size = 6 + iv_base64_len + uid_base64_len + data_base64_len;
    encrypt_data[pos++] = (payload_size >> 8) & 0xFF;
    encrypt_data[pos++] = payload_size & 0xFF;
	//payload
	//iv_len iv
    encrypt_data[pos++] = (iv_base64_len >> 8) & 0xFF;
    encrypt_data[pos++] = iv_base64_len & 0xFF;
    memcpy(encrypt_data + pos, iv_base64, iv_base64_len);
    pos += iv_base64_len;
	//uuid_len uuid
    encrypt_data[pos++] = (uid_base64_len >> 8) & 0xFF;
    encrypt_data[pos++] = uid_base64_len & 0xFF;
    memcpy(encrypt_data + pos, uid_base64, uid_base64_len);
    pos += uid_base64_len;
	//data_len data
    encrypt_data[pos++] = (data_base64_len >> 8) & 0xFF;
    encrypt_data[pos++] = data_base64_len & 0xFF;
    memcpy(encrypt_data + pos, data_base64, data_base64_len);
    pos += data_base64_len;

    *encrypt_data_len = pos;
	ENTITY_LOGI("pos:%d\r\n", pos);
    //Dump_Hex_To_String(LOG_LEVEL_DEBUG,"encrypt_data:", encrypt_data, pos);
	//ENTITY_LOGD("\r\n encrypt_data str:%s\r\n", encrypt_data);
    return 0;
}



/**
*@名称 		Entity_Tcp_Respone_Parse
*@功能 		解析TCP应答数据
*@参数 		const unsigned char *auth_data, int auth_data_len, char *aes_key, Entity_Tcp_Respone_Decrypt_t *result
*@返回值 	int
*@使用说明	
*/
int Entity_Tcp_Respone_Parse(const unsigned char *auth_data, int auth_data_len, char *aes_key, Entity_Tcp_Respone_Decrypt_t *result)
{
 //为了节省内存，而共用缓存
	if (auth_data == NULL)
        return -1;
    ENTITY_LOGI("%s\r\n", __func__);
    int pos = 0;
	
	//头部5字节
    result->Version = auth_data[pos++];//协议版本号
    result->Type = auth_data[pos++];//3-唤醒包
    result->Flag = auth_data[pos++];//0-不加密(心跳，唤醒)
    int payload_size = (auth_data[pos] << 8) | auth_data[pos + 1];
    pos += 2;
	//ENTITY_LOGI("[%s]: auth_data_len:%d, payload_size:%d\r\n", __func__, auth_data_len, payload_size);
	if(payload_size > auth_data_len-5)
	{
		ENTITY_LOGW("%s, payload_size too large\r\n", __func__);
		return -1;
	}

	//1、iv base64解码
    int iv_len = (auth_data[pos] << 8) | auth_data[pos + 1];
    pos += 2;
#ifdef NEED_CONSERVE_RAM
	//为节省栈空间，先把返回缓存区拿来用
    memcpy(result->Data, &auth_data[pos], iv_len);
	result->Data[iv_len] = '\0';
    pos += iv_len;
	//ENTITY_LOGD("[%s]: iv_len:%d\r\n", __func__,iv_len);
    unsigned char iv[IV_LEN];//栈17字节
    size_t iv_len_decoded;
    mbedtls_base64_decode(iv, sizeof(iv), &iv_len_decoded, (unsigned char*)result->Data, iv_len);
	//ENTITY_LOGD("%s, after base64 decode: iv_len_decoded:%d\n", __func__, iv_len_decoded);//解码后的长度
#else	
    unsigned char iv_base64[iv_len + 1];
    memcpy(iv_base64, &auth_data[pos], iv_len);
    iv_base64[iv_len] = '\0';
    pos += iv_len;
    unsigned char iv[IV_LEN];
    size_t iv_len_decoded;
    mbedtls_base64_decode(iv, sizeof(iv), &iv_len_decoded, iv_base64, iv_len);
#endif

	//2、uuid base64解码
    int uid_len = (auth_data[pos] << 8) | auth_data[pos + 1];
    pos += 2;
#ifdef NEED_CONSERVE_RAM
	//为节省栈空间，先把返回缓存区拿来用
    memcpy(result->Data, &auth_data[pos], uid_len);
	result->Data[uid_len] = '\0';
    pos += uid_len;
	//ENTITY_LOGD("%s: uid_len:%d, result->Data:%s\r\n", __func__, uid_len, result->Data);
    unsigned char uid_decoded[128];//栈128字节
    size_t uid_len_decoded;
    mbedtls_base64_decode(uid_decoded, sizeof(uid_decoded), &uid_len_decoded, (unsigned char*)result->Data, uid_len);
	//ENTITY_LOGD("[%s]:uuid after base64_decode uid_len_decoded:%d\r\n", __func__, uid_len_decoded);
#else
    unsigned char uid_base64[uid_len + 1];
    memcpy(uid_base64, &auth_data[pos], uid_len);
    uid_base64[uid_len] = '\0';
    pos += uid_len;
    unsigned char uid_decoded[128];
    size_t uid_len_decoded;
    mbedtls_base64_decode(uid_decoded, sizeof(uid_decoded), &uid_len_decoded, uid_base64, uid_len);
#endif

    //3、uuid aes解密
	Aes_Decrypt(uid_decoded, uid_len_decoded, (unsigned char*)aes_key, (char *)iv, result->Uuid);
	uid_len = Pkcs7_Cuttingg((char*)result->Uuid, uid_len_decoded);//去掉填充
	//ENTITY_LOGD("uuid after Aes_Decrypt [%s]: uid_len：%d, response->Uuid = %s\n", __func__, uid_len, result->Uuid);
   
	//4、数据 base64解码
	int data_len = (auth_data[pos] << 8) | auth_data[pos + 1];
    pos += 2;
#ifdef NEED_CONSERVE_RAM
	//为节省栈空间，先把返回缓存区拿来用
	memcpy(result->Data, &auth_data[pos], data_len);
	result->Data[data_len] = '\0';
	pos += data_len;
	//ENTITY_LOGD("[%s]: data_len:%d\r\n", __func__, data_len);
    unsigned char data_decoded[QUICK_INFO_LEN_MAX];
    size_t data_decoded_len;
    mbedtls_base64_decode(data_decoded, sizeof(data_decoded), &data_decoded_len, (unsigned char*)result->Data, data_len);
	//ENTITY_LOGD("[%s]:data after base64_decode, data_decoded_len:%d\r\n", __func__, data_decoded_len);
	memset(result->Data, 0, data_len+1);//复位缓存区
#else
    unsigned char data_base64[data_len + 1];
    memcpy(data_base64, &auth_data[pos], data_len);
    data_base64[data_len] = '\0';
    pos += data_len;
    unsigned char data_decoded[QUICK_INFO_LEN_MAX];
    size_t data_decoded_len;
    mbedtls_base64_decode(data_decoded, sizeof(data_decoded), &data_decoded_len, data_base64, data_len);
#endif
	//5、数据 aes解密
	Aes_Decrypt(data_decoded, data_decoded_len, (unsigned char*)aes_key,(char *)iv, (unsigned char*)result->Data);
	data_len = Pkcs7_Cuttingg((char*)result->Data, data_decoded_len);
	result->Data_Len = data_len;
	ENTITY_LOGD("data after Aes_Decrypt [%s]: Data_Len:%d, response->Data = %s\r\n", __func__, result->Data_Len, result->Data);

    return 0;
}


/**
*@名称 		Entity_Get_Auth_Encryption_Data
*@功能 		获取鉴权加密数据
*@参数 		unsigned char *enc_data, int *enc_data_len
*@返回值 	int
*@使用说明	
*/
int Entity_Get_Auth_Encryption_Data(char *uuid, char *secret, char *aes_key, unsigned char *enc_data, int *enc_data_len)
{
	Entity_Get_Encryption_Data(KEEPALIVE_CMD_TYPE_AUTH_REQUEST, NULL, uuid, secret, aes_key, enc_data, enc_data_len);
	return 0;
}

/**
*@名称 		Entity_Get_Heartbeat_Encryption_Data
*@功能 		获取心跳加密数据
*@参数 		unsigned char flag_bat_low_report,char *uuid, char *secret, 
*@参数 		char *aes_key, unsigned char *enc_data, int *enc_data_len
*@返回值 	int
*@使用说明	
*/
int Entity_Get_Heartbeat_Encryption_Data(unsigned char flag_bat_low_report, char *uuid, char *secret, \
                                        char *aes_key, unsigned char *enc_data, int *enc_data_len)
{
#ifdef TCP_HEARTBEAT_USE_V2
	char costom_str[128]={0};
	
	if(flag_bat_low_report)//需要上报低电，此标志在心跳应答中清
	{
		sprintf(costom_str, "\"dp_map\":{\"low_battery\":true}");
		Entity_Get_Encryption_Data(KEEPALIVE_CMD_TYPE_HEARTBEAT, costom_str, uuid, secret, aes_key,enc_data, enc_data_len);
	}
	else
	{
		Entity_Get_Encryption_Data(KEEPALIVE_CMD_TYPE_HEARTBEAT, NULL, uuid, secret, aes_key, enc_data, enc_data_len);
	}
#else
	enc_data[0] = ENTITY_HEARTBEAT_VERSION;
	enc_data[1] = ENTITY_HEARTBEAT_TYPE;
	enc_data[2] = ENTITY_HEARTBEAT_FLAG;
	enc_data[3] = 0;
	enc_data[4] = 0;
	*enc_data_len = 5;
#endif
	return 0;
}



/**
*@名称 		Entity_Auth_Resp_Parse
*@功能 		解析鉴权应答数据
*@参数 		const unsigned char *auth_data, int auth_data_len, char *aes_key, Entity_Tcp_Respone_Decrypt_t *result
*@返回值 	int
*@使用说明	
*/
int Entity_Auth_Resp_Parse(const unsigned char *auth_data, int auth_data_len, char *aes_key, Entity_Tcp_Respone_Decrypt_t *result)
{
	int ret = 0;
	ret =  Entity_Tcp_Respone_Parse(auth_data, auth_data_len, aes_key, result);
	char err_str[10];
	int err_str_len = Parse_Json_Field(result->Data, "\"err\"", err_str);
	if(err_str_len > 0)
	{
		ENTITY_LOGD("err_str:%s\r\n", err_str);
	}
	char interval_str[10];
	int interval_str_len = Parse_Json_Field(result->Data, "\"interval\"", interval_str);
	if(interval_str_len > 0)
	{
		ENTITY_LOGD("interval_str:%s\r\n", interval_str);
	}
	char time_str[20];
	int time_str_len = Parse_Json_Field(result->Data, "\"time\"", time_str);
	if(time_str_len > 0)
	{
		ENTITY_LOGD("time_str:%s\r\n", time_str);
	}
	char random_str[30];
	int random_str_len = Parse_Json_Field(result->Data, "\"random\"", random_str);
	if(random_str_len > 0)
	{
		ENTITY_LOGD("random_str:%s\r\n", random_str);
	}
	char signature_str[50];
	int signature_str_len = Parse_Json_Field(result->Data, "\"signature\"", signature_str);
	if(signature_str_len > 0)
	{
		ENTITY_LOGD("signature_str:%s\r\n", signature_str);
	}
	
    if(err_str_len && interval_str_len && time_str_len && random_str_len && signature_str_len)
    {
        result->Error = atoi(err_str);
        result->Interval = atoi(interval_str);
        strncpy(result->Time, time_str, sizeof(result->Time));
        strncpy(result->Random, random_str, sizeof(result->Random));
        strncpy(result->Signature, signature_str, sizeof(result->Signature));
    }
	return ret;
}

/**
*@名称 		Entity_Keepalive_Wakeup_Resp_Parse
*@功能 		解析唤醒数据
*@参数 		const unsigned char *wakeup_data, int wakeup_data_len, char *aes_key,Entity_Tcp_Respone_Decrypt_t *result
*@返回值 	int
*@使用说明	
*/
int Entity_Keepalive_Wakeup_Resp_Parse(const unsigned char *wakeup_data, int wakeup_data_len, char *aes_key,Entity_Tcp_Respone_Decrypt_t *result)
{
	ENTITY_LOGI("%s\r\n", __func__);
	return Entity_Tcp_Respone_Parse(wakeup_data, wakeup_data_len, aes_key, result);
}


/**
*@名称 		Entity_Keepalive_Heartbeat_Resp_Parse
*@功能 		解析心跳应答数据
*@参数 		const unsigned char *rsp_data, int rsp_data_len, v Entity_Tcp_Respone_Decrypt_t *result
*@返回值 	int 0-无低电心跳应答   1-低电心跳应答 
*@使用说明	
*/
int Entity_Keepalive_Heartbeat_Resp_Parse(const unsigned char *rsp_data, int rsp_data_len, char *aes_key, Entity_Tcp_Respone_Decrypt_t *result)
{
	int ret = 0;
	ENTITY_LOGI("%s\r\n", __func__);
	ret =  Entity_Tcp_Respone_Parse(rsp_data, rsp_data_len, aes_key, result);
	
	char time_str[20];
	int time_str_len = Parse_Json_Field(result->Data, "\"time\"", time_str);
	if(time_str_len > 0)
	{
		ENTITY_LOGD("time_str:%s\r\n", time_str);
	}
	char random_str[30];
	int random_str_len = Parse_Json_Field(result->Data, "\"random\"", random_str);
	if(random_str_len > 0)
	{
		ENTITY_LOGD("random_str:%s\r\n", random_str);
	}
	char low_battery_str[30];
	int low_battery_str_len = Parse_Json_Field(result->Data, "\"low_battery\"", low_battery_str);
	if(low_battery_str_len > 0)
	{
		ENTITY_LOGD("low_battery:%s\r\n", low_battery_str);
	}
	
    if(time_str_len && random_str_len)
    {
        result->Error = 0;
        strncpy(result->Time, time_str, sizeof(result->Time));
		//更新RTC时间
		uint32_t stamp = atoi(time_str);
		Entity_Set_Time_Stamp(stamp);
		if(strncmp(low_battery_str, "true", 4) == 0)
		{
            return 1;
		}
    }
	return ret;
}
