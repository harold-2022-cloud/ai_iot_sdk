#pragma once 

#define CFG_USE_BASE64        1

#ifdef CFG_USE_BASE64

//计算字符串的base64编码长度
unsigned int Base64_Calc_Encode_Length(unsigned int src_len);

//BASE64 编码
unsigned char Base64_Encode(const unsigned char *src, int len, int *out_len, unsigned char *out);

//计算字符串base64解码后的长度
unsigned int Base64_Calc_Decode_Length(const unsigned char *src, unsigned int src_len);

//base64解码
unsigned char Base64_Decode(const unsigned char *src, int len, int *out_len, unsigned char *out);



#endif /*CFG_USE_BASE64*/


