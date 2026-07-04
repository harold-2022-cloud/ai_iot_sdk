//factory_test_peripheral.h
#pragma once


typedef struct 
{
    void (*Test_Mic)(void);
    void (*Test_Speaker)(void);
    void (*Test_Led_Indicator)(void);
    void (*Test_Lcd_Display)(void);
    void (*Test_Motor)(void);
    void (*Test_Nfc)(char *card, unsigned char *card_len);
    void (*Test_Gyro)(unsigned char *data, unsigned char *data_len);
}Entity_Factory_Test_Func_t;

//厂测接口函数初始化
void Entity_Factory_Test_Func_Init(Entity_Factory_Test_Func_t *cbs);

//获取厂测接口函数
Entity_Factory_Test_Func_t *Get_Entity_Factory_Test_Func(void);

//手动测试MIC
void Factory_Manual_Test_Mic(void);

//手动测试喇叭
void Factory_Manual_Test_Speaker(void);

//手动测试LED
void Factory_Manual_Test_Led_Indicator(void);

//手动测试LCD显示
void Factory_Manual_Test_Lcd_Display(void);

//手动测试马达
void Factory_Manual_Test_Motor(void);

//手动测试NFC
void Factory_Manual_Test_Nfc(char *card, unsigned char *card_len);

//手动测试陀螺仪
void Factory_Manual_Test_Gyro(unsigned char *data, unsigned char *data_len);

