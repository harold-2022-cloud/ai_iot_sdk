#include "base_64.h"
#include <string.h>

#if CFG_USE_BASE64

#include "bsp_include.h"


#define BASE64_ZALLOC   Bsp_Psram_Zalloc
#define BASE64_FREE     Bsp_Psram_Free

#define DTABLE_LEN         256

/* ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/ */
static const unsigned char base_64_table[64] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
												'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
												'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a',
												'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
												'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
												't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1',
												'2', '3', '4', '5', '6', '7', '8', '9', '+',
												'/'
											   };

/**
*@名称 		Base64_Calc_Encode_Length
*@功能 		计算字符串的base64编码长度
*@参数 		unsigned int src_len
*@返回值 	unsigned int
*@使用说明	
*/
unsigned int Base64_Calc_Encode_Length(unsigned int src_len)
{
	unsigned int enc_len = 0;
	enc_len = src_len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
	enc_len += enc_len / 72; /* line feeds */
	enc_len++; /* nul termination ('\0') */
	return enc_len;
}


/**
*@名称 		Base64_Encode
*@功能 		BASE64 编码
*@参数 		const unsigned char *src, int len, int *out_len, unsigned char *out
*@返回值 	unsigned char
*@使用说明	1-成功 0-失败
*/
unsigned char Base64_Encode(const unsigned char *src, int len, int *out_len, unsigned char *out)
{
	unsigned char  *pos;
	const unsigned char *end, *in;
	int line_len;

	if (out == ((unsigned char *)0))
		return 0;

	end = src + len;
	in = src;
	pos = out;
	line_len = 0;
	while (end - in >= 3) {
		*pos++ = base_64_table[in[0] >> 2];
		*pos++ = base_64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
		*pos++ = base_64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
		*pos++ = base_64_table[in[2] & 0x3f];
		in += 3;
		line_len += 4;
		if (line_len >= 72) {
			*pos++ = '\n';
			line_len = 0;
		}
	}

	if (end - in) {
		*pos++ = base_64_table[in[0] >> 2];
		if (end - in == 1) {
			*pos++ = base_64_table[(in[0] & 0x03) << 4];
			*pos++ = '=';
		} else {
			*pos++ = base_64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
			*pos++ = base_64_table[(in[1] & 0x0f) << 2];
		}
		*pos++ = '=';
		line_len += 4;
	}

	if (line_len)
		*pos++ = '\n';

	*pos = '\0';
	if (out_len)
		(*out_len) = pos - out;
	return 1;
}

/**
*@名称 		Base64_Calc_Encode_Length
*@功能 		计算字符串base64解码后的长度
*@参数 		const unsigned char *src, unsigned int src_len
*@返回值 	unsigned int
*@使用说明	
*/
unsigned int Base64_Calc_Decode_Length(const unsigned char *src, unsigned int src_len)
{
	unsigned int dec_len = 0;
	unsigned char *dtable = NULL;
	unsigned int i;

	dtable = (unsigned char *)BASE64_ZALLOC(DTABLE_LEN);
	if (NULL == dtable)
		return 0;

	memset(dtable, 0x80, 256);
	for (i = 0; i < sizeof(base_64_table); i++)
		dtable[base_64_table[i]] = i;
	dtable['='] = 0;

	dec_len = 0;
	for (i = 0; i < src_len; i++) {
		if (dtable[src[i]] != 0x80)
			dec_len++;
	}
	if (dec_len % 4)
		dec_len = 0;

	BASE64_FREE(dtable);
	return dec_len;
}

/**
*@名称 		Base64_Decode
*@功能 		base64解码
*@参数 		const unsigned char *src, int len, int *out_len, unsigned char *out
*@返回值 	unsigned char
*@使用说明	1-成功 0-失败
*/
unsigned char Base64_Decode(const unsigned char *src, int len, int *out_len, unsigned char *out)
{
	unsigned char *pos, in[4], block[4], tmp;
	unsigned int i, count;
	unsigned char *dtable = NULL;

	dtable = (unsigned char *)BASE64_ZALLOC(DTABLE_LEN);
	if (NULL == dtable) {
		(*out_len) = 0;
		return 0;
	}

	memset(dtable, 0x80, 256);
	for (i = 0; i < sizeof(base_64_table); i++)
		dtable[base_64_table[i]] = i;
	dtable['='] = 0;

	count = 0;
	for (i = 0; i < len; i++) {
		if (dtable[src[i]] != 0x80)
			count++;
	}

	if (count % 4) {
		BASE64_FREE(dtable);
		return 0;
	}
	if (out == ((unsigned char *)0)) {
		*out_len = (count * 3) / 4;
		BASE64_FREE(dtable);
		return 0;
	}
	pos = out;

	count = 0;
	for (i = 0; i < len; i++) {
		tmp = dtable[src[i]];
		if (tmp == 0x80)  //'\n'
			continue;

		in[count] = src[i];
		block[count] = tmp;
		count++;
		if (count == 4) {
			*pos++ = (block[0] << 2) | (block[1] >> 4);
			*pos++ = (block[1] << 4) | (block[2] >> 2);
			*pos++ = (block[2] << 6) | block[3];
			count = 0;
		}
	}

	if (pos > out) {
		if (in[2] == '=')
			pos -= 2;
		else if (in[3] == '=')
			pos--;
	}

	(*out_len) = pos - out;

	BASE64_FREE(dtable);
	return 1;
}
#endif
// eof

