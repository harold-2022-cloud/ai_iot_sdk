//factory_test_peripheral.c
#include "factory_test_peripheral.h"


Entity_Factory_Test_Func_t Entity_Factory_Test_Func;



/**
*@名称 		Get_Entity_Factory_Test_Func
*@功能 		获取厂测接口函数
*@参数 		void
*@返回值 	Entity_Factory_Test_Func_t *
*@使用说明	
*/
Entity_Factory_Test_Func_t *Get_Entity_Factory_Test_Func(void)
{
    return &Entity_Factory_Test_Func;
}

/**
*@名称 		Entity_Factory_Test_Func_Init
*@功能 		厂测接口函数初始化
*@参数 		Entity_Factory_Test_Func_t *cbs
*@返回值 	void
*@使用说明	
*/
void Entity_Factory_Test_Func_Init(Entity_Factory_Test_Func_t *cbs)
{
    Entity_Factory_Test_Func = *cbs;
}




/**
*@名称 		Factory_Manual_Test_Mic
*@功能 		手动测试MIC
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Factory_Manual_Test_Mic(void)
{
    if(Entity_Factory_Test_Func.Test_Mic)
        Entity_Factory_Test_Func.Test_Mic();
}

/**
*@名称 		Factory_Manual_Test_Speaker
*@功能 		手动测试喇叭
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Factory_Manual_Test_Speaker(void)
{
    if(Entity_Factory_Test_Func.Test_Speaker)
        Entity_Factory_Test_Func.Test_Speaker();
}

/**
*@名称 		Factory_Manual_Test_Led_Indicator
*@功能 		手动测试LED
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Factory_Manual_Test_Led_Indicator(void)
{
    if(Entity_Factory_Test_Func.Test_Led_Indicator)
        Entity_Factory_Test_Func.Test_Led_Indicator();
}

/**
*@名称 		Factory_Manual_Test_Lcd_Display
*@功能 		手动测试LCD显示
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Factory_Manual_Test_Lcd_Display(void)
{
    if(Entity_Factory_Test_Func.Test_Lcd_Display)
        Entity_Factory_Test_Func.Test_Lcd_Display();
}

/**
*@名称 		Factory_Manual_Test_Motor
*@功能 		手动测试马达
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Factory_Manual_Test_Motor(void)
{
    if(Entity_Factory_Test_Func.Test_Motor)
        Entity_Factory_Test_Func.Test_Motor();
}

/**
*@名称 		Factory_Manual_Test_Nfc
*@功能 		手动测试NFC
*@参数 		char *card, unsigned char *card_len
*@返回值 	void
*@使用说明	
*/
void Factory_Manual_Test_Nfc(char *card, unsigned char *card_len)
{
    if(Entity_Factory_Test_Func.Test_Nfc)
        Entity_Factory_Test_Func.Test_Nfc(card, card_len);
}

/**
*@名称 		Factory_Manual_Test_Gyro
*@功能 		手动测试陀螺仪
*@参数 		unsigned char *data, unsigned char *data_len
*@返回值 	void
*@使用说明	
*/
void Factory_Manual_Test_Gyro(unsigned char *data, unsigned char *data_len)
{
    if(Entity_Factory_Test_Func.Test_Gyro)
        Entity_Factory_Test_Func.Test_Gyro(data, data_len);
}






