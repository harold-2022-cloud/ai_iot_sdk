//com_crc.h
#pragma once


#define CRC_UINT32_MAX      0xFFFFFFFF
#define CRC_UINT16_MAX      0xFFFF
#define CRC_UINT8_MAX       0xFF

#define CRC_UINT32_MIN      0
#define CRC_UINT16_MIN      0
#define CRC_UINT8_MIN       0



//CRC8校验
unsigned char Com_Crc8(unsigned char crc, const unsigned char *data, unsigned int length) ;

//CRC16校验 Modbus
unsigned short Com_Crc16_Modbus(unsigned short crc, unsigned char *data, unsigned int length);

//CRC16校验 Ccitt
unsigned short Com_Crc16_Ccitt(unsigned short crc, const unsigned char *data, unsigned int length) ;

//分段CRC校验计算
unsigned int Com_Section_Crc32(unsigned int crc, const unsigned char *data, unsigned int length, unsigned char is_last) ;

//CRC32校验计算
unsigned int Com_Crc32(unsigned int crc, const unsigned char *data, unsigned int length) ;

//计算校验和
unsigned char Check_Sum(unsigned char *pack, unsigned short pack_len);

//异或校验 
unsigned char Xor_Inverted_Check(unsigned char *buf, unsigned int len);


