#include "base_64.h"

static const unsigned char k_base64_table[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int decode_char(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z')
    {
        return ch - 'a' + 26;
    }
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0' + 52;
    }
    if (ch == '+')
    {
        return 62;
    }
    if (ch == '/')
    {
        return 63;
    }
    if (ch == '=')
    {
        return 0;
    }
    return -1;
}

unsigned int Base64_Calc_Encode_Length(unsigned int src_len)
{
    return ((src_len + 2u) / 3u) * 4u + 1u;
}

unsigned char Base64_Encode(const unsigned char *src, int len, int *out_len, unsigned char *out)
{
    int pos = 0;
    int i;

    if (src == ((const unsigned char *)0) || out == ((unsigned char *)0) || len < 0)
    {
        return 0;
    }
    for (i = 0; i < len; i += 3)
    {
        int remain = len - i;
        unsigned int value = ((unsigned int)src[i] << 16) |
                             ((remain > 1 ? (unsigned int)src[i + 1] : 0u) << 8) |
                             (remain > 2 ? (unsigned int)src[i + 2] : 0u);

        out[pos++] = k_base64_table[(value >> 18) & 0x3Fu];
        out[pos++] = k_base64_table[(value >> 12) & 0x3Fu];
        out[pos++] = (remain > 1) ? k_base64_table[(value >> 6) & 0x3Fu] : '=';
        out[pos++] = (remain > 2) ? k_base64_table[value & 0x3Fu] : '=';
    }
    out[pos] = '\0';
    if (out_len != ((int *)0))
    {
        *out_len = pos;
    }
    return 1;
}

unsigned int Base64_Calc_Decode_Length(const unsigned char *src, unsigned int src_len)
{
    unsigned int padding = 0;

    if (src == ((const unsigned char *)0) || src_len == 0u)
    {
        return 0;
    }
    if (src[src_len - 1u] == '=')
    {
        padding++;
    }
    if (src_len > 1u && src[src_len - 2u] == '=')
    {
        padding++;
    }
    return (src_len / 4u) * 3u - padding;
}

unsigned char Base64_Decode(const unsigned char *src, int len, int *out_len, unsigned char *out)
{
    int pos = 0;
    int i;

    if (src == ((const unsigned char *)0) || out == ((unsigned char *)0) ||
        out_len == ((int *)0) || len < 0 || (len % 4) != 0)
    {
        return 0;
    }
    for (i = 0; i < len; i += 4)
    {
        int a = decode_char(src[i]);
        int b = decode_char(src[i + 1]);
        int c = decode_char(src[i + 2]);
        int d = decode_char(src[i + 3]);
        unsigned int value;

        if (a < 0 || b < 0 || c < 0 || d < 0)
        {
            return 0;
        }
        value = ((unsigned int)a << 18) | ((unsigned int)b << 12) | ((unsigned int)c << 6) | (unsigned int)d;
        out[pos++] = (unsigned char)((value >> 16) & 0xFFu);
        if (src[i + 2] != '=')
        {
            out[pos++] = (unsigned char)((value >> 8) & 0xFFu);
        }
        if (src[i + 3] != '=')
        {
            out[pos++] = (unsigned char)(value & 0xFFu);
        }
    }
    *out_len = pos;
    return 1;
}
