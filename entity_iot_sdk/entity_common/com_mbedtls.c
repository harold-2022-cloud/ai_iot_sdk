//com_mbedtls.c
#include "com_mbedtls.h"

#include <string.h>
#include "com_utils.h"

#include "entity_log.h"
#include "entity_iot_func.h"



/**
*@名称 		Pkcs7_Padding
*@功能 		pkcs7填充
*@参数 		char *data,     数据
*@参数 		int dataSize,   存储 data的数组容量
*@参数 		int dataLen     data 的长度
*@返回值 	int -1 data数据为空 2 填充后的数据越界 >0 填充后的数据长度
*@使用说明	
*/
int Pkcs7_Padding(char *data, int dataSize, int dataLen)
{
    if (dataLen <= 0)
        return -1;

    if ((dataLen + 17) >= dataSize)
        return -2;
    uint8_t paddingNum = 0;
    paddingNum = 16 - (dataLen % 16);
    memset(&data[dataLen], paddingNum, paddingNum);

    data[dataLen + paddingNum] = '\0';
    return dataLen + paddingNum;
}

/**
*@名称 		Pkcs7_Cuttingg
*@功能 		pkcs7去除填充
*@参数 		char *data,     数据
*@参数 		int dataLen     data 的长度
*@返回值 	int -1 data数据为空 2 填充后的数据越界 >0 填充后的数据长度
*@使用说明	
*/
int Pkcs7_Cuttingg(char *data, int dataLen)
{
    if (dataLen <= 0)
        return -1;
    uint8_t paddingNum = data[dataLen - 1];
    // check
    int i;
    for (i = 0; i < paddingNum; i++)
    {
        if (data[dataLen - paddingNum + i] != paddingNum)
            return -3;
    }
    memset(&data[dataLen - paddingNum], 0, paddingNum);
    data[dataLen - paddingNum] = '\0';
    return dataLen - paddingNum;
}

/**
*@名称 		Aes_Encrypt
*@功能 		aes加密
*@参数 		unsigned char *data, int length, unsigned char *aes_key, char *iv, unsigned char *data_out
*@返回值 	void
*@使用说明	
*/
void Aes_Encrypt(unsigned char *data, int length, unsigned char *aes_key, char *iv, unsigned char *data_out)
{
	unsigned char aes_key_bytes[16];
    Hexstr_To_Bytes((const char*)aes_key, aes_key_bytes, sizeof(aes_key_bytes));

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aes_key_bytes, 128);
    unsigned char iv_copy[16];
    memcpy(iv_copy, iv, 16);
	mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, iv_copy, data, data_out);
    mbedtls_aes_free(&aes);
}


/**
*@名称 		Aes_Decrypt
*@功能 		aes解密
*@参数 		unsigned char *data_enc, int length, char *iv, unsigned char *aes_key, unsigned char *data_out
*@返回值 	void
*@使用说明	
*/
void Aes_Decrypt(unsigned char *data_enc, int length, unsigned char *aes_key, char *iv, unsigned char *data_out)
{
	unsigned char aes_key_bytes[16];
    Hexstr_To_Bytes((const char*)aes_key, aes_key_bytes, sizeof(aes_key_bytes));
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, aes_key_bytes, 128);
    unsigned char iv_copy[16];
    memcpy(iv_copy, iv, 16);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv_copy, data_enc, data_out);
    mbedtls_aes_free(&aes);
}


/**
*@名称 		Hmac_Sha1
*@功能 		对字符串进行sha1加密
*@参数 		const char *str, const char *secret, unsigned char *digest
*@返回值 	int
*@使用说明	
*/
int Hmac_Sha1(const char *str, const char *secret, unsigned char *digest)
{

	int str_len = strlen(str);
    int secret_len = strlen(secret);
    const mbedtls_md_info_t *md_info;
    mbedtls_md_context_t ctx;
     // 初始化 md 上下文
    mbedtls_md_init(&ctx);
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (!md_info || mbedtls_md_setup(&ctx, md_info, 1) != 0)
    { // 启用 HMAC 模式
        mbedtls_md_free(&ctx);
        return -1;
    }
    // 计算 HMAC
    if (mbedtls_md_hmac_starts(&ctx, (const unsigned char *)secret, secret_len) != 0 ||
        mbedtls_md_hmac_update(&ctx, (const unsigned char *)str, str_len) != 0 ||
        mbedtls_md_hmac_finish(&ctx, digest) != 0)
    {
        mbedtls_md_free(&ctx);
        return -1;
    }
    mbedtls_md_free(&ctx);
    return SHA1_DIGEST_SIZE;
}


/**
*@名称 		Mbedtls_Hmac
*@功能 		hmac加密
*@参数 		mbedtls_md_type_t md_type,  类型
*@参数 		unsigned char *key,         密钥
*@参数 		size_t keylen,              密钥长度
*@参数 		unsigned char *data,        被加密数据
*@参数 		size_t datalen,             被加密数据长度
*@参数 		unsigned char *output       加密后数据
*@返回值 	int 0成功 非0失败
*@使用说明	
 */
int Mbedtls_Hmac(mbedtls_md_type_t md_type, unsigned char *key, size_t keylen, unsigned char *data, size_t datalen, unsigned char *output)
{
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info;
    int rc = 0;

    if ((key == NULL) || (data == NULL) || (output == NULL))
    {
        return -1;
    }

    md_info = mbedtls_md_info_from_type(md_type);
    if (md_info == NULL)
    {
        return -1;
    }

    memset(output, 0, mbedtls_md_get_size(md_info));

    mbedtls_md_init(&ctx);
    rc = mbedtls_md_setup(&ctx, md_info, 1);
    if (rc != 0)
    {
        goto exit;
    }

    rc = mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, keylen);
    if (rc != 0)
    {
        goto exit;
    }

    rc = mbedtls_md_hmac_update(&ctx, (const unsigned char *)data, datalen);
    if (rc != 0)
    {
        goto exit;
    }

    rc = mbedtls_md_hmac_finish(&ctx, output);

exit:
    if (rc != 0)
    {
        ENTITY_LOGE("Mbedtls_Hmac failed, rc=%d\r\n", rc);
    }

    mbedtls_md_free(&ctx);
    return rc;
}


/**
*@名称 		Mbedtls_Aes_Ecb
*@功能 		AES加密
*@参数 		unsigned char mode,  类型
*@参数 		unsigned char padding         
*@参数 		const unsigned char *key    密钥
*@参数 		unsigned int keybits        
*@参数 		const unsigned char *data, 
*@参数 		int length    
*@参数      unsigned char *out_buf  解密后的数据
*@返回值 	unsigned int 解密后的数据长度
*@使用说明	
*/
unsigned int Mbedtls_Aes_Ecb(unsigned char mode, unsigned char padding, const unsigned char *key, unsigned int keybits, const unsigned char *data, int length, unsigned char *out_buf)
{
    unsigned int out_length=0;
    mbedtls_aes_context aes_ctx;
   
    mbedtls_aes_init(&aes_ctx);
    switch (mode)
    {
    case MBEDTLS_AES_ENCRYPT:
        mbedtls_aes_setkey_enc(&aes_ctx, key, keybits);
        break;
    case MBEDTLS_AES_DECRYPT:
        mbedtls_aes_setkey_dec(&aes_ctx, key, keybits);
        break;
    default:
        ENTITY_LOGE("Please select MBEDTLS_AES_ENCRYPT/MBEDTLS_AES_DECRYPT\r\n");
        mbedtls_aes_free(&aes_ctx);
        return out_length;
    }
    int remainder = 16 - (length % 16);
    int total_length = ((remainder == 16) && (padding == MBEDTLS_AES_CRYPT_ZERO_PADDING)) ? length : (length + remainder);
    if(!out_buf)
    {
        out_buf = (unsigned char *)Entity_Mem_Malloc(total_length);
        if (out_buf == NULL)
        {
            ENTITY_LOGE(" %s out_buf malloc failed!!\r\n", __func__);
            mbedtls_aes_free(&aes_ctx);
            return out_length;
        }
    }

    out_length = total_length;
    memset(out_buf, 0, total_length);
    for (int i = 0; i < (total_length / 16); i++)
    {
        unsigned char pack_buff[16] = {0};
        unsigned short pack_len = length - (i * 16);
        unsigned char *data_addr = (unsigned char *)&data[i * 16];

        if (pack_len >= 16)
        {
            memcpy(pack_buff, data_addr, 16);
        }
        else
        {
            memcpy(pack_buff, data_addr, pack_len);
            if (padding == MBEDTLS_AES_CRYPT_PKCS7_PADDING)
            {
                memset(pack_buff + pack_len, remainder, remainder);
            }
        }

        mbedtls_aes_crypt_ecb(&aes_ctx, mode, pack_buff, &out_buf[i * 16]);
    }

    mbedtls_aes_free(&aes_ctx);

    return out_length;
}



/**
*@名称 		Mbedtls_Base64_Encode
*@功能 		base64编码
*@参数 		const unsigned char *src    源数据
*@参数 		size_t len                  源数据长度
*@参数 		size_t *out_len             编码长度
*@返回值 	unsigned char *             编码后数据
*@使用说明	使用完后要释放返回值内存
*/
unsigned char *Mbedtls_Base64_Encode(const unsigned char *src, size_t len, size_t *out_len)
{
    int ret = mbedtls_base64_encode(NULL, 0, out_len, src, len); 
    if(ret != 0 && ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        ENTITY_LOGE("[%s][%d]length err! :%d\r\n", __func__, __LINE__, ret);
        return NULL;
    }   

    unsigned char *base64 = (unsigned char *)Entity_Mem_Malloc(*out_len);
    if(base64 == NULL) {
        ENTITY_LOGE("[%s][%d]malloc err!\r\n", __func__, __LINE__);
        return NULL;
    }
    mbedtls_base64_encode(base64, *out_len, out_len, src, len);
    return base64;
}

/**
*@名称 		Mbedtls_Base64_Decode
*@功能 		base64解码
*@参数 		const unsigned char *src    源数据
*@参数 		size_t len                  源数据长度
*@参数 		size_t *out_len             编码长度
*@返回值 	unsigned char *             编码后数据
*@使用说明	使用完后要释放返回值内存
*/
unsigned char *Mbedtls_Base64_Decode(const unsigned char *src, size_t len, size_t *out_len)
{
    int ret = mbedtls_base64_decode(NULL, 0, out_len, src, len); 
    if(ret != 0 && ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        ENTITY_LOGE("[%s][%d]length err! :%d\r\n", __func__, __LINE__, ret);
        return NULL;
    }   

    unsigned char *base64 = (unsigned char *)Entity_Mem_Malloc(*out_len);
    if(base64 == NULL) {
        ENTITY_LOGE("[%s][%d]malloc err!\r\n", __func__, __LINE__);
        return NULL;
    }
    mbedtls_base64_decode(base64, *out_len, out_len, src, len);
    return base64;
}




/**
*@名称 		Mbedtls_Md5_Process
*@功能 		MD5加密
*@参数 		const unsigned char *content
*@参数 		unsigned short content_len         
*@参数 		const char out[32]
*@返回值 	void
*@使用说明	
*/
void Mbedtls_Md5_Process(const unsigned char *content, unsigned short content_len, const char out[32])
{
    mbedtls_md5_context md5_ctx;
    unsigned char decrypt[16] = {0};

    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);
    mbedtls_md5_update(&md5_ctx, content, content_len);
    mbedtls_md5_finish(&md5_ctx, decrypt);
    mbedtls_md5_free(&md5_ctx);
    Hex_Array_To_String(decrypt, sizeof(decrypt), (char *)out);
}

/**
*@名称 		Mbedtls_Md5_Init
*@功能 		MD5加密初始化
*@参数 		void
*@返回值 	void*
*@使用说明	
*/
void *Mbedtls_Md5_Init(void)
{
    mbedtls_md5_context *ctx = Entity_Mem_Malloc(sizeof(mbedtls_md5_context));
    if (NULL == ctx) {
        return NULL;
    }
    mbedtls_md5_init(ctx);
    mbedtls_md5_starts(ctx);
    return ctx;
}

/**
*@名称 		Mbedtls_Md5_Update
*@功能 		MD5加密更新
*@参数 		void *md5, const char *buf, size_t buf_len
*@返回值 	void
*@使用说明	
*/
void Mbedtls_Md5_Update(void *md5, const char *buf, size_t buf_len)
{
    mbedtls_md5_update((mbedtls_md5_context*)md5, (unsigned char *)buf, buf_len);
}
    
/**
*@名称 		Mbedtls_Md5_Finish
*@功能 		MD5加密完成 
*@参数 		void *md5, char *output_str
*@返回值 	void
*@使用说明	
*/
void Mbedtls_Md5_Finish(void *md5, char *output_str)
{
    unsigned char buf_out[16]={0};
    mbedtls_md5_finish((mbedtls_md5_context*)md5, buf_out);
    Hex_Array_To_String(buf_out, sizeof(buf_out), output_str);
    output_str[32] = '\0';
}

/**
*@名称 		Mbedtls_Md5_Deinit
*@功能 		MD5加密反初始化
*@参数 		void *md5
*@返回值 	void
*@使用说明	
*/
void Mbedtls_Md5_Deinit(void *md5)
{
    if (NULL != md5) 
    {
        Entity_Mem_Free(md5);
    }
}   

/**
*@名称 		Mbedtls_Md5_Reset
*@功能 		MD5加密复位
*@参数 		void *md5
*@返回值 	int
*@使用说明	
*/
int Mbedtls_Md5_Reset(void *md5)
{
    Mbedtls_Md5_Deinit(md5);
    md5 = Mbedtls_Md5_Init();
    if(md5 == NULL)
        return -1;
    return 0;
}
    

   










