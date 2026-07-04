//com_mbedtls.h
#pragma once

#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/md5.h"

#define SHA1_DIGEST_SIZE        20

#define MBEDTLS_AES_CRYPT_ZERO_PADDING      0
#define MBEDTLS_AES_CRYPT_PKCS7_PADDING     1


typedef struct
{
    unsigned char *data;
    unsigned short length;
} aes_crypt_t;

//pkcs7填充
int Pkcs7_Padding(char *data, int dataSize, int dataLen);

//pkcs7去除填充
int Pkcs7_Cuttingg(char *data, int dataLen);

//aes加密
void Aes_Encrypt(unsigned char *data, int length, unsigned char *aes_key, char *iv, unsigned char *data_out);

//aes解密
void Aes_Decrypt(unsigned char *data_enc, int length, unsigned char *aes_key, char *iv, unsigned char *data_out);

//对字符串进行sha1加密
int Hmac_Sha1(const char *str, const char *secret, unsigned char *digest);

//hmac加密
int Mbedtls_Hmac(mbedtls_md_type_t md_type, unsigned char *key, size_t keylen, unsigned char *data, size_t datalen, unsigned char *output);

//AES加密
unsigned int Mbedtls_Aes_Ecb(unsigned char mode, unsigned char padding, const unsigned char *key, unsigned int keybits, const unsigned char *data, int length, unsigned char *out_buf);

//MBASE64
unsigned char *Mbedtls_Base64_Encode(const unsigned char *src, size_t len, size_t *out_len);

//base64解码
unsigned char *Mbedtls_Base64_Decode(const unsigned char *src, size_t len, size_t *out_len);

//MD5加密
void Mbedtls_Md5_Process(const unsigned char *content, unsigned short content_len, const char out[32]);

//MD5加密初始化
void *Mbedtls_Md5_Init(void);

//MD5加密更新
void Mbedtls_Md5_Update(void *md5, const char *buf, size_t buf_len);

//MD5加密完成
void Mbedtls_Md5_Finish(void *md5, char *output_str);

//MD5加密反初始化
void Mbedtls_Md5_Deinit(void *md5);

//MD5加密复位
int Mbedtls_Md5_Reset(void *md5);


