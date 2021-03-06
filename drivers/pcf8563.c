/**
  *****************************************************************************
  *                            时钟芯片PCF8563驱动
  *
  *                       (C) Copyright 2000-2020, ***
  *                            All Rights Reserved
  *****************************************************************************
  *
  * @File    : pcf8563.c
  * @Version : V1.0
  * @By      : Sam Chan
  * @Date    : 2012 / 08 / 28
  * @Brief   :
  *
  *****************************************************************************
  *                                   Update
  * @Version : V1.0.1
  * @By      : Sam Chan
  * @Date    : 2013 / 10 / 20
  * @Brief   : A、增加显示时间日期格式数组
  *            B、增加读取时间处理函数，读取到的时间日期信息直接转换成ASCII保存到时间格式数组中
  *            C、调用时间日期处理函数，显示或者打印到串口的话直接显示或者打印时间格式数组即可
  *
  * @Version : V1.0.2
  * @By      : Sam Chan
  * @Date    : 2014 / 02 / 26
  * @Brief   : 修正年结构为16位数值，数值位，比如20xx、19xx
  *
  * @Version : V1.0.3
  * @By      : Sam Chan
  * @Date    : 2014 / 03 / 09
  * @Brief   : 增加PCF8563是否存在检测函数
  *
  * @Version : V1.0.4
  * @By      : Sam Chan
  * @Date    : 2014 / 05 / 10
  * @Brief   : A、增加导入默认参数函数，方便移植
  *            B、增加对C++环境的支持
  *
  * @Version : V1.0.5
  * @By      : Sam Chan
  * @Date    : 2014 / 07 / 19
  * @Brief   : A、修正显示时间时bug，显示字符后有乱码现象或者显示了不该显示的字符
  *            B、增加直接设置时间函数，方便用类似于USMART这样的工具直接调整
  *
  * @Version : V2.0
  * @By      : Sam
  * @Date    : 2015 / 05 / 15
  * @Brief   : 全面修改代码，增加带BIN和BCD转换功能
  *
  * @Version : V3.0
  * @By      : DDL
  * @Date    : 2017-9-13
  * @Brief   : A、增加GPIO宏定义；
  *            B、增加PCF8563_Test函数；
  *            C、增加PCF8563_Init函数；
  *            D、删除世纪位的设置、读取与判断；
  *
  *****************************************************************************
**/

/**************************************************************************************************************
											Header file includes
**************************************************************************************************************/
#include "pcf8563.h"

/**************************************************************************************************************
										 Global variable definition
**************************************************************************************************************/
#define DBG_ENABLE
#define DBG_SECTION_NAME "rtc.pcf8563"
#define DBG_LEVEL DBG_LOG
#define DBG_COLOR

#include <rtdbg.h>
#ifndef LOG_D
#error "Please update the 'rtdbg.h' file to GitHub latest version (https://github.com/RT-Thread/rt-thread/blob/master/include/rtdbg.h)"
#endif
/**************************************************************************************************************
											Function definition
**************************************************************************************************************/
unsigned char buffer[10];
/**
  *****************************************************************************
  * @Func   : 将BIN转换为BCD
  *
  * @Brief  : none
  *
  * @Input  : BINValue:----二进制格式数据
  *
  * @Output : none
  *
  * @Return : BCD格式数值
  *****************************************************************************
**/
static unsigned char RTC_BinToBcd2(unsigned char BINValue)
{
    unsigned char bcdhigh = 0;

    while (BINValue >= 10)
    {
        bcdhigh++;
        BINValue -= 10;
    }

    return ((unsigned char)(bcdhigh << 4) | BINValue);
}
/**
  *****************************************************************************
  * @Func   : 将BCD转换为BIN
  *
  * @Brief  : none
  *
  * @Input  : BCDValue----二进码十进数格式数据
  *
  * @Output : none
  *
  * @Return : BIN格式数值
  *****************************************************************************
**/
static unsigned char RTC_Bcd2ToBin(unsigned char BCDValue)
{
    unsigned char tmp = 0;

    tmp = ((unsigned char)(BCDValue & (unsigned char)0xF0) >> (unsigned char)0x04) * 10;

    return (tmp + (BCDValue & (unsigned char)0x0F));
}

/******************************************************************************
* Function Name --> IIC_GPIO_Init
* Description   --> GPIO初始化
* Input         --> none
* Output        --> none
* Reaturn       --> none 
******************************************************************************/
void IIC_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(IIC_SCL_PORT_RCC | IIC_SDA_PORT_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉

    GPIO_InitStructure.GPIO_Pin = IIC_SCL_PIN;
    GPIO_Init(IIC_SCL_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = IIC_SDA_PIN;
    GPIO_Init(IIC_SDA_PORT, &GPIO_InitStructure);

    IIC_SDA = 1; //置IIC总线空闲
    IIC_SCL = 1;
}

/******************************************************************************
* Function Name --> SDA_OUT
* Description   --> SDA输出配置	
* Input         --> none
* Output        --> none
* Reaturn       --> none 
******************************************************************************/
void IIC_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
    GPIO_InitStructure.GPIO_Pin = IIC_SDA_PIN;
    GPIO_Init(IIC_SDA_PORT, &GPIO_InitStructure);
}

/******************************************************************************
* Function Name --> SDA_IN
* Description   --> SDA输入配置
* Input         --> none
* Output        --> none
* Reaturn       --> none 
******************************************************************************/
void IIC_SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; //上拉
    GPIO_InitStructure.GPIO_Pin = IIC_SDA_PIN;
    GPIO_Init(IIC_SDA_PORT, &GPIO_InitStructure);
}

/******************************************************************************
* Function Name --> IIC启动
* Description   --> SCL高电平期间，SDA由高电平突变到低电平时启动总线
*                   SCL: __________
*                                  \__________
*                   SDA: _____
*                             \_______________
* Input         --> none
* Output        --> none
* Reaturn       --> none 
******************************************************************************/
void IIC_Start(void)
{
    IIC_SDA_OUT(); //设置成输出

    IIC_SDA = 1; //为SDA下降启动做准备
    IIC_SCL = 1; //在SCL高电平时，SDA为下降沿时候总线启动

#if _USER_DELAY_CLK == 1 /* 定义了则使用延时函数来改变通讯频率 */

    IIC_Delay();
    IIC_SDA = 0; //突变，总线启动
    IIC_Delay();
    IIC_SCL = 0;
    IIC_Delay();

#else /* 否则不使用延时函数改变通讯频率 */

    IIC_SDA = 0; //突变，总线启动
    IIC_SCL = 0;

#endif /* end __USER_DELAY_CLK */
}

/******************************************************************************
* Function Name --> IIC停止
* Description   --> SCL高电平期间，SDA由低电平突变到高电平时停止总线
*                   SCL: ____________________
*                                  __________
*                   SDA: _________/
* Input         --> none
* Output        --> none
* Reaturn       --> none 
******************************************************************************/
void IIC_Stop(void)
{
    IIC_SDA_OUT(); //设置成输出

    IIC_SDA = 0; //为SDA上升做准备

#if _USER_DELAY_CLK == 1 /* 定义了则使用延时函数来改变通讯频率 */

    IIC_Delay();
    IIC_SCL = 1; //在SCL高电平时，SDA为上升沿时候总线停止
    IIC_Delay();
    IIC_SDA = 1; //突变，总线停止
    IIC_Delay();

#else /* 否则不使用延时函数改变通讯频率 */

    IIC_SCL = 1; //在SCL高电平时，SDA为上升沿时候总线停止
    IIC_SDA = 1; //突变，总线停止

#endif /* end __USER_DELAY_CLK */
}

/******************************************************************************
* Function Name --> 主机向从机发送应答信号
* Description   --> none
* Input         --> a：应答信号
*                      0：应答信号
*                      1：非应答信号
* Output        --> none
* Reaturn       --> none 
******************************************************************************/
void IIC_Ack(unsigned char a)
{
    IIC_SDA_OUT(); //设置成输出

    if (a)
        IIC_SDA = 1; //放上应答信号电平
    else
        IIC_SDA = 0;

#if _USER_DELAY_CLK == 1 /* 定义了则使用延时函数来改变通讯频率 */

    IIC_Delay();
    IIC_SCL = 1; //为SCL下降做准备
    IIC_Delay();
    IIC_SCL = 0; //突变，将应答信号发送过去
    IIC_Delay();

#else /* 否则不使用延时函数改变通讯频率 */

    IIC_SCL = 1; //为SCL下降做准备
    IIC_SCL = 0; //突变，将应答信号发送过去

#endif /* end __USER_DELAY_CLK */
}
/******************************************************************************
* Function Name --> 主机等待接收slave ACK
* Description   --> none
* Input         --> 
* Output        --> none
* Reaturn       --> none 
******************************************************************************/
unsigned char IIC_Wait_Ack(void)
{
    unsigned char i = 0;
    IIC_SDA_IN(); //设置成输入
    IIC_SDA = 1;  //放上应答信号电平
    IIC_Delay();
    IIC_SCL = 1; //为SCL下降做准备
    IIC_Delay();
    IIC_SDA_IN(); //设置成输入

    //---等待应答信号
    while (IN_SDA)
    {
        i++;
        if (i > 250)
        {
            IIC_Stop();
            return 1;
        }
    }
    IIC_SCL = 0; //为SCL下降做准备
    return 0;
}

/******************************************************************************
* Function Name --> 向IIC总线发送一个字节数据
* Description   --> none
* Input         --> dat：要发送的数据
* Output        --> none
* Reaturn       --> ack：返回应答信号
******************************************************************************/
unsigned char IIC_Write_Byte(unsigned char dat)
{
    unsigned char i;
    unsigned char iic_ack = 0; //iic应答标志

    IIC_SDA_OUT(); //设置成输出

    for (i = 0; i < 8; i++)
    {
        if (dat & 0x80)
            IIC_SDA = 1; //判断发送位，先发送高位
        else
            IIC_SDA = 0;

#if _USER_DELAY_CLK == 1 /* 定义了则使用延时函数来改变通讯频率 */

        IIC_Delay();
        IIC_SCL = 1; //为SCL下降做准备
        IIC_Delay();
        IIC_SCL = 0; //突变，将数据位发送过去
        dat <<= 1;   //数据左移一位
    }                //字节发送完成，开始接收应答信号

    IIC_SDA = 1; //释放数据线

    IIC_SDA_IN(); //设置成输入

    IIC_Delay();
    IIC_SCL = 1; //为SCL下降做准备
    IIC_Delay();

#else /* 否则不使用延时函数改变通讯频率 */

        IIC_SCL = 1; //为SCL下降做准备
        IIC_SCL = 0; //突变，将数据位发送过去
        dat <<= 1;   //数据左移一位
    }                //字节发送完成，开始接收应答信号

    IIC_SDA = 1; //释放数据线

    IIC_SDA_IN(); //设置成输入

    IIC_SCL = 1; //为SCL下降做准备

#endif /* end __USER_DELAY_CLK */
    while (IN_SDA)
    {
        i++;
        IIC_Delay();
        if (i > 250)
            break;
    }
    iic_ack |= IN_SDA; //读入应答位
    IIC_SCL = 0;
    return iic_ack; //返回应答信号
}

/******************************************************************************
* Function Name --> 从IIC总线上读取一个字节数据
* Description   --> none
* Input         --> none
* Output        --> none
* Reaturn       --> x：读取到的数据
******************************************************************************/
unsigned char IIC_Read_Byte(void)
{
    unsigned char i;
    unsigned char x = 0;

    IIC_SDA = 1; //首先置数据线为高电平

    IIC_SDA_IN(); //设置成输入

    for (i = 0; i < 8; i++)
    {
        x <<= 1; //读入数据，高位在前

#if _USER_DELAY_CLK == 1 /* 定义了则使用延时函数来改变通讯频率 */

        IIC_Delay();
        IIC_SCL = 1; //突变
        IIC_Delay();

        if (IN_SDA)
            x |= 0x01; //收到高电平

        IIC_SCL = 0;
        IIC_Delay();
    } //数据接收完成

#else /* 否则不使用延时函数改变通讯频率 */

        IIC_SCL = 1; //突变

        if (IN_SDA)
            x |= 0x01; //收到高电平

        IIC_SCL = 0;
    } //数据接收完成

#endif /* end __USER_DELAY_CLK */

    IIC_SCL = 0;

    return x; //返回读取到的数据
}

/**
  *****************************************************************************
  * @Func   : PCF8563测试函数
  *
  * @Brief  : 向PCF8563写入一个初始时间，然后循环读取时间并在串口助手显示
  *
  * @Input  : none
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
int PCF8563_Config(void)
{
    _PCF8563_Date_Typedef PCF8563_Date_Structure;
    _PCF8563_Time_Typedef PC8563F_Time_Structure;

    PCF8563_Init();       //PCF8563初始化
    rt_thread_delay(100); //延时一段时间

    if (PCF8563_Check())
        LOG_W("PCF8563 Error"); //PCF8563检测结果：错误或损坏
    else
        LOG_W("PCF8563 OK"); //PCF8563检测结果：正常

    PCF8563_Date_Structure.RTC_Years = 19;    //初始化年
    PCF8563_Date_Structure.RTC_Months = 01;   //初始化月
    PCF8563_Date_Structure.RTC_WeekDays = 03; //初始化周
    PCF8563_Date_Structure.RTC_Days = 9;      //初始化日

    PC8563F_Time_Structure.RTC_Hours = 05;   //初始化时
    PC8563F_Time_Structure.RTC_Minutes = 06; //初始化分
    PC8563F_Time_Structure.RTC_Seconds = 07; //初始化秒

    PCF8563_SetMode(PCF_Mode_Normal); //设置模式
    PCF8563_Stop();                   //PCF8563停止
    // PCF8563_SetDate(PCF_Format_BIN, &PCF8563_Date_Structure); //设置日期
    // PCF8563_SetTime(PCF_Format_BIN, &PC8563F_Time_Structure); //设置时间
    PCF8563_Start(); //PCF8563启动

    PCF8563_GetTime(PCF_Format_BCD, &PC8563F_Time_Structure); //获取时间
    PCF8563_GetDate(PCF_Format_BCD, &PCF8563_Date_Structure); //获取日期

    LOG_I("Greenwich Time: 20%02x-%02x-%02x %02x:%02x:%02x",
          PCF8563_Date_Structure.RTC_Years,    //显示年
          PCF8563_Date_Structure.RTC_Months,   //显示月
          PCF8563_Date_Structure.RTC_Days,     //显示日
          PC8563F_Time_Structure.RTC_Hours,    //显示时
          PC8563F_Time_Structure.RTC_Minutes,  //显示分
          PC8563F_Time_Structure.RTC_Seconds); //显示秒

    return 0;
}
/**
  *****************************************************************************
  * @Func   : PCF8563初始化
  *
  * @Brief  : 配置PCF8563引脚为开漏输出，并置I2C总线空闲
  *
  * @Input  : none
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_Init(void)
{
    IIC_GPIO_Init();
}
/**
  *****************************************************************************
  * @Func   : PCF8563检测
  *
  * @Brief  : 向定时器倒计时寄存器写入一个数值再读取出来做对比，相同正确，不同则错误
  *
  * @Input  : none
  *
  * @Output : none
  *
  * @Return : 0: 正常
  *           1: PCF8563错误或者损坏
  *****************************************************************************
**/
unsigned char PCF8563_Check(void)
{
    unsigned char test_value = 0;
    unsigned char Time_Count = 0; //定时器倒计时数据缓存

    if (PCF8563_Read_Byte(PCF8563_Address_Timer) & 0x80) //如果打开了定时器，则先关闭
    {
       PCF8563_Write_Byte(PCF8563_Address_Timer, PCF_Timer_Close); //先关闭定时器
        Time_Count = PCF8563_Read_Byte(PCF8563_Address_Timer_VAL);  //先保存计数值
    }

    PCF8563_Write_Byte(PCF8563_Address_Timer_VAL, PCF8563_Check_Data); //写入检测值
    for (test_value = 0; test_value < 250; test_value++)
    {
    }                                                          //延时一定时间再读取
    test_value = PCF8563_Read_Byte(PCF8563_Address_Timer_VAL); //再读取回来

    if (Time_Count != 0) //启动了定时器功能，则恢复
    {
       PCF8563_Write_Byte(PCF8563_Address_Timer_VAL, Time_Count); //恢复现场
        PCF8563_Write_Byte(PCF8563_Address_Timer, PCF_Timer_Open); //启动定时器
    } 
  
    if (test_value != PCF8563_Check_Data) //如果读出值与写入值不同，则器件错误或损坏
    {
        return 1; //错误或损坏
    }

    return 0; //正常
}
/**
  *****************************************************************************
  * @Func   : PCF8563写入一个字节数据
  *
  * @Brief  : none
  *
  * @Input  : REG_ADD----要操作寄存器地址
  *           dat--------要写入的数据
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_Write_Byte(unsigned char REG_ADD, unsigned char dat)
{
    IIC_Start(); //I2C启动

    if (!(IIC_Write_Byte(PCF8563_Write))) //发送写命令并检查应答位
    {
        IIC_Write_Byte(REG_ADD); //确定接下来要操作的寄存器
        IIC_Write_Byte(dat);     //发送数据
    }

    IIC_Stop(); //I2C停止
}
/**
  *****************************************************************************
  * @Func   : PCF8563读取一个字节数据
  *
  * @Brief  : none
  *
  * @Input  : REG_ADD----要操作寄存器地址
  *
  * @Output : none
  *
  * @Return : read_data----读取得到的寄存器的值
  *****************************************************************************
**/
unsigned char PCF8563_Read_Byte(unsigned char REG_ADD)
{
    unsigned char read_data;

    IIC_Start(); //I2C启动

    if (!(IIC_Write_Byte(PCF8563_Write))) //发送写命令并检查应答位
    {
        IIC_Write_Byte(REG_ADD); //确定接下来要操作的寄存器

        IIC_Start();                  //I2C重启
        IIC_Write_Byte(PCF8563_Read); //发送读命令并检查应答位
        read_data = IIC_Read_Byte();  //读取数据
        IIC_Ack(1);                   //发送非应答信号结束数据传送
    }

    IIC_Stop(); //I2C停止

    return read_data;
}
/**
  *****************************************************************************
  * @Func   : PCF8563写入多组数据
  *
  * @Brief  : none
  *
  * @Input  : REG_ADD----要操作寄存器起始地址
  *           num--------写入数据数量
  *           *WBuff-----写入数据缓存
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_Write_nByte(unsigned char REG_ADD, unsigned char num, unsigned char *pBuff)
{
    unsigned char i = 0;

    IIC_Start(); //I2C启动

    if (!(IIC_Write_Byte(PCF8563_Write))) //发送写命令并检查应答位
    {
        IIC_Write_Byte(REG_ADD); //确定接下来要操作的寄存器
        for (i = 0; i < num; i++)
        {
            IIC_Write_Byte(*pBuff); //写入数据
            pBuff++;
        }
    }

    IIC_Stop(); //I2C停止
}
/**
  *****************************************************************************
  * @Func   : PCF8563读取多组数据
  *
  * @Brief  : none
  *
  * @Input  : REG_ADD----要操作寄存器起始地址
  *           num--------读取数据数量
  *
  * @Output : *WBuff-----读取数据缓存
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_Read_nByte(unsigned char REG_ADD, unsigned char num, unsigned char *pBuff)
{
    unsigned char i = 0;

    IIC_Start(); //I2C启动

    if (!(IIC_Write_Byte(PCF8563_Write))) //发送写命令并检查应答位
    {
        IIC_Write_Byte(REG_ADD); //确定接下来要操作的寄存器

        IIC_Start();                  //I2C重启
        IIC_Write_Byte(PCF8563_Read); //发送读命令并检查应答位
        for (i = 0; i < num; i++)
        {
            *pBuff = IIC_Read_Byte(); //读取数据
            if (i == (num - 1))
                IIC_Ack(1); //发送非应答信号
            else
                IIC_Ack(0); //发送应答信号
            pBuff++;
        }
    }

    IIC_Stop(); //I2C停止
}
/**
  *****************************************************************************
  * @Func   : PCF8563启动
  *
  * @Brief  : none
  *
  * @Input  : none
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_Start(void)
{
    unsigned char temp = 0;

    temp = PCF8563_Read_Byte(PCF8563_Address_Control_Status_1); //读取控制/状态寄存器1

    if (temp & PCF_Control_ChipStop)
    {
        temp &= PCF_Control_ChipRuns; //运行芯片
    }

    if ((temp & (1 << 7)) == 0) //普通模式
    {
        temp &= PCF_Control_TestcClose; //电源复位模式失效
    }

    PCF8563_Write_Byte(PCF8563_Address_Control_Status_1, temp); //再写入数值
}
/**
  *****************************************************************************
  * @Func   : PCF8563停止
  *
  * @Brief  : 时钟频率输出CLKOUT 在 32.768kHz 时可用
  *
  * @Input  : none
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_Stop(void)
{
    unsigned char temp = 0;

    temp = PCF8563_Read_Byte(PCF8563_Address_Control_Status_1); //读取控制/状态寄存器1
    temp |= PCF_Control_ChipStop;                               //停止运行
    PCF8563_Write_Byte(PCF8563_Address_Control_Status_1, temp); //再写入数值
}
/**
  *****************************************************************************
  * @Func   : PCF8563设置运行模式
  *
  * @Brief  : none
  *
  * @Input  : Mode: 运行模式
  *                 = PCF_Mode_Normal
  *                 = PCF_Mode_EXT_CLK
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_SetMode(unsigned char Mode)
{
    unsigned char temp = 0;

    temp = PCF8563_Read_Byte(PCF8563_Address_Control_Status_1); //读取寄存器值
    if (Mode == PCF_Mode_EXT_CLK)                               //EXT_CLK测试模式
    {
        temp |= PCF_Control_Status_EXT_CLKMode;
    }
    else if (Mode == PCF_Mode_Normal)
    {
        temp &= PCF_Control_Status_NormalMode;
        temp &= ~(1 << 3); //电源复位功能失效
    }
    PCF8563_Write_Byte(PCF8563_Address_Control_Status_1, temp);
}
/**
  *****************************************************************************
  * @Func   : PCF8563设置电源复位功能开启与关闭
  *
  * @Brief  : none
  *
  * @Input  : NewState: 状态，PCF8563_PowerResetEnablePCF8563_PowerResetDisable
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_SetPowerReset(unsigned char NewState)
{
    unsigned char TestC = 0;

    TestC = PCF8563_Read_Byte(PCF8563_Address_Control_Status_1); //获取寄存器值
    TestC &= ~(1 << 3);                                          //清除之前设置
    if (NewState == PCF8563_PowerResetEnable)                    //复位功能有效
    {
        TestC |= PCF8563_PowerResetEnable;
    }
    else if (NewState == PCF8563_PowerResetDisable)
    {
        TestC &= ~PCF8563_PowerResetEnable; //失效，普通模式是值逻辑0，即失效
    }
    PCF8563_Write_Byte(PCF8563_Address_Control_Status_1, TestC); //写入数值
}
/**
  *****************************************************************************
  * @Func   : PCF8563设置输出频率
  *
  * @Brief  : none
  *
  * @Input  :*PCF_CLKOUTStruct: 频率结构指针
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_SetCLKOUT(_PCF8563_CLKOUT_Typedef *PCF_CLKOUTStruct)
{
    unsigned char tmp = 0;

    tmp = PCF8563_Read_Byte(PCF8563_Address_CLKOUT); //读取寄存器值
    tmp &= 0x7c;                                     //清除之前设置
    if (PCF_CLKOUTStruct->CLKOUT_NewState == PCF_CLKOUT_Open)
    {
        tmp |= PCF_CLKOUT_Open;
    }
    else
    {
        tmp &= PCF_CLKOUT_Close;
    }
    tmp |= PCF_CLKOUTStruct->CLKOUT_Frequency;

    PCF8563_Write_Byte(PCF8563_Address_CLKOUT, tmp);
}
/**
  *****************************************************************************
  * @Func   : PCF8563设置定时器
  *
  * @Brief  : none
  *
  * @Input  :*PCF_TimerStruct: 定时器结构指针
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_SetTimer(_PCF8563_Timer_Typedef *PCF_TimerStruct)
{
    unsigned char Timer_Ctrl = 0;
    unsigned char Timer_Value = 0;

    Timer_Ctrl = PCF8563_Read_Byte(PCF8563_Address_Timer);      //获的控制寄存器值
    Timer_Value = PCF8563_Read_Byte(PCF8563_Address_Timer_VAL); //获取倒计时数值
    //
    //先停止定时器
    //
    Timer_Ctrl &= PCF_Timer_Close;
    PCF8563_Write_Byte(PCF8563_Address_Timer, Timer_Ctrl);

    Timer_Ctrl &= 0x7c; //清除定时器之前设置

    if (PCF_TimerStruct->RTC_Timer_NewState == PCF_Timer_Open) //开启
    {
        Timer_Ctrl |= PCF_Timer_Open;
        Timer_Ctrl |= PCF_TimerStruct->RTC_Timer_Frequency; //填上新的工作频率
        if (PCF_TimerStruct->RTC_Timer_Value)               //需要填上新的计数值
        {
            Timer_Value = PCF_TimerStruct->RTC_Timer_Value; //填上新的计数值
        }
    }
    else
    {
        Timer_Ctrl &= PCF_Timer_Close;
    }
    PCF8563_Write_Byte(PCF8563_Address_Timer_VAL, Timer_Value); //写入倒计时数值

    if (PCF_TimerStruct->RTC_Timer_Interrupt == PCF_Time_INT_Open) //开启了中断输出
    {
        Timer_Value = PCF8563_Read_Byte(PCF8563_Address_Control_Status_2); //获取控制/状态寄存器2数值
        Timer_Value &= PCF_Time_INT_Close;                                 //清除定时器中断使能
        Timer_Value &= ~(1 << 2);                                          //清除定时器中断标志
        Timer_Value &= ~(1 << 4);                                          //当 TF 有效时 INT 有效 (取决于 TIE 的状态)
        Timer_Value |= PCF_Time_INT_Open;                                  //开启定时器中断输出
        PCF8563_Write_Byte(PCF8563_Address_Control_Status_2, Timer_Value);
    }
    else
    {
        Timer_Value = PCF8563_Read_Byte(PCF8563_Address_Control_Status_2); //获取控制/状态寄存器2数值
        Timer_Value &= PCF_Time_INT_Close;                                 //清除定时器中断使能
        Timer_Value |= PCF_Time_INT_Open;                                  //开启定时器中断输出
        PCF8563_Write_Byte(PCF8563_Address_Control_Status_2, Timer_Value);
    }

    PCF8563_Write_Byte(PCF8563_Address_Timer, Timer_Ctrl); //设置定时器控制寄存器
}
/**
  *****************************************************************************
  * @Func   : 设置时间，主要用于后台调用，或者初始化时间用
  *
  * @Brief  : 秒默认就设置成0x00了，形参里面不体现，星期值范围：0 ~ 6
  *
  * @Input  : PCF_Format:  数据格式
  *                        = PCF_Format_BIN
  *                        = PCF_Format_BCD
  *           PCF_Century: 世纪位设定
  *                        = PCF_Century_19xx
  *                        = PCF_Century_20xx
  *           Year:        年
  *           Month:       月
  *           Date:        日
  *           Week:        星期
  *           Hour:        时
  *           Minute:      分
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_Set_Times(unsigned char PCF_Format, unsigned char Year, unsigned char Month, unsigned char Date, unsigned char Week, unsigned char Hour, unsigned char Minute)
{
    _PCF8563_Time_Typedef Time_InitStructure;
    _PCF8563_Date_Typedef Date_InitStructure;

    if (PCF_Format == PCF_Format_BIN)
    {
        //判断数据是否符合范围
        if (Year > 99)
            Year = 0; //恢复00年
        if (Month > 12)
            Month = 1; //恢复1月
        if (Date > 31)
            Date = 1; //恢复1日
        if (Week > 6)
            Week = 1; //恢复星期一

        if (Hour > 23)
            Hour = 0; //恢复0小时
        if (Minute > 59)
            Minute = 0; //恢复0分钟

        //转换一下
        Date_InitStructure.RTC_Years = RTC_BinToBcd2(Year);
        Date_InitStructure.RTC_Months = RTC_BinToBcd2(Month);
        Date_InitStructure.RTC_Days = RTC_BinToBcd2(Date);
        Date_InitStructure.RTC_WeekDays = RTC_BinToBcd2(Week);

        Time_InitStructure.RTC_Hours = RTC_BinToBcd2(Hour);
        Time_InitStructure.RTC_Minutes = RTC_BinToBcd2(Minute);
    }
    Time_InitStructure.RTC_Seconds = 0x00;                   //恢复0秒
    Time_InitStructure.RTC_Seconds &= PCF_Accuracy_ClockYes; //保证准确的时间
    //写入信息到寄存器
    buffer[0] = Time_InitStructure.RTC_Seconds;
    buffer[1] = Time_InitStructure.RTC_Minutes;
    buffer[2] = Time_InitStructure.RTC_Hours;
    PCF8563_Write_nByte(PCF8563_Address_Seconds, 3, buffer); //写入时间信息

    buffer[0] = Date_InitStructure.RTC_Days;
    buffer[1] = Date_InitStructure.RTC_WeekDays;
    buffer[2] = Date_InitStructure.RTC_Months;
    buffer[3] = Date_InitStructure.RTC_Years;
    PCF8563_Write_nByte(PCF8563_Address_Days, 4, buffer); //写入日期信息
}

/**
   ============================================================================
                     #### 所有寄存器全部操作功能函数 ####
   ============================================================================
**/

/**
  *****************************************************************************
  * @Func   : 判断时间信息是否符合范围，超出将恢复初值
  *
  * @Brief  : 星期值范围：0 ~ 6
  *
  * @Input  : PCF_DataStruct: 寄存器结构指针
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
static void IS_PCF8563_Data(_PCF8563_Register_Typedef *PCF_DataStruct)
{
    if (PCF_DataStruct->Years > 99)
        PCF_DataStruct->Years = 0; //恢复00年
    if (PCF_DataStruct->Months_Century > 12)
        PCF_DataStruct->Months_Century = 1; //恢复1月
    if (PCF_DataStruct->Days > 31)
        PCF_DataStruct->Days = 1; //恢复1日
    if (PCF_DataStruct->WeekDays > 6)
        PCF_DataStruct->WeekDays = 1; //恢复星期一

    if (PCF_DataStruct->Hours > 23)
        PCF_DataStruct->Hours = 0; //恢复0小时
    if (PCF_DataStruct->Minutes > 59)
        PCF_DataStruct->Minutes = 0; //恢复0分钟
    if (PCF_DataStruct->Seconds > 59)
        PCF_DataStruct->Seconds = 0; //恢复0秒
}
/**
  *****************************************************************************
  * @Func   : PCF8563写入寄存器
  *
  * @Brief  : 星期数值范围是: 0 ~ 6，十进制格式
  *
  * @Input  : PCF_Format:     数据格式
  *                           = PCF_Format_BIN
  *                           = PCF_Format_BCD
  *           PCF_DataStruct: 寄存器结构指针
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_SetRegister(unsigned char PCF_Format, _PCF8563_Register_Typedef *PCF_DataStruct)
{
    if (PCF_Format == PCF_Format_BIN) //十进制格式，需要转换一下
    {
        //判断数值是否在范围之内
        IS_PCF8563_Data(PCF_DataStruct);

        //需要转换一下
        PCF_DataStruct->Years = RTC_BinToBcd2(PCF_DataStruct->Years);
        PCF_DataStruct->Months_Century = RTC_BinToBcd2(PCF_DataStruct->Months_Century);
        PCF_DataStruct->Days = RTC_BinToBcd2(PCF_DataStruct->Days);
        PCF_DataStruct->WeekDays = RTC_BinToBcd2(PCF_DataStruct->WeekDays);

        PCF_DataStruct->Hours = RTC_BinToBcd2(PCF_DataStruct->Hours);
        PCF_DataStruct->Minutes = RTC_BinToBcd2(PCF_DataStruct->Minutes);
        PCF_DataStruct->Seconds = RTC_BinToBcd2(PCF_DataStruct->Seconds);

        PCF_DataStruct->Day_Alarm = RTC_BinToBcd2(PCF_DataStruct->Day_Alarm);
        PCF_DataStruct->WeekDays_Alarm = RTC_BinToBcd2(PCF_DataStruct->WeekDays_Alarm);

        PCF_DataStruct->Hour_Alarm = RTC_BinToBcd2(PCF_DataStruct->Hour_Alarm);
        PCF_DataStruct->Minute_Alarm = RTC_BinToBcd2(PCF_DataStruct->Minute_Alarm);
    }

    //关闭所有闹铃，中断输出
    PCF_DataStruct->Timer_Control &= ~(1 << 7);
    PCF_DataStruct->CLKOUT_Frequency &= ~(1 << 7);
    PCF_DataStruct->WeekDays_Alarm &= ~(1 << 7);
    PCF_DataStruct->Day_Alarm &= ~(1 << 7);
    PCF_DataStruct->Hour_Alarm &= ~(1 << 7);
    PCF_DataStruct->Minute_Alarm &= ~(1 << 7);
    PCF_DataStruct->Control_Status_2 &= ~(3 << 0);

    //写入数据到寄存器
    PCF8563_Write_nByte(PCF8563_Address_Control_Status_1, 16, (unsigned char *)PCF_DataStruct);
}
/**
  *****************************************************************************
  * @Func   : PCF8563读取寄存器
  *
  * @Brief  : 星期数值范围是: 0 ~ 6，十进制格式
  *
  * @Input  : PCF_Format:     数据格式
  *                           = PCF_Format_BIN
  *                           = PCF_Format_BCD
  *
  * @Output : *PCF_Century:   世纪位，0：21世纪；1:20世纪
  *           PCF_DataStruct: 寄存器结构指针
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_GetRegister(unsigned char PCF_Format, _PCF8563_Register_Typedef *PCF_DataStruct)
{
    //读取全部寄存器数值
    PCF8563_Read_nByte(PCF8563_Address_Control_Status_1, 16, (unsigned char *)PCF_DataStruct);

    //屏蔽无效位
    PCF_DataStruct->Years &= PCF8563_Shield_Years;
    PCF_DataStruct->Months_Century &= PCF8563_Shield_Months_Century;
    PCF_DataStruct->Days &= PCF8563_Shield_Days;
    PCF_DataStruct->WeekDays &= PCF8563_Shield_WeekDays;

    PCF_DataStruct->Hours &= PCF8563_Shield_Hours;
    PCF_DataStruct->Minutes &= PCF8563_Shield_Minutes;
    PCF_DataStruct->Seconds &= PCF8563_Shield_Seconds;

    PCF_DataStruct->Minute_Alarm &= PCF8563_Shield_Minute_Alarm;
    PCF_DataStruct->Hour_Alarm &= PCF8563_Shield_Hour_Alarm;
    PCF_DataStruct->Day_Alarm &= PCF8563_Shield_Day_Alarm;
    PCF_DataStruct->WeekDays_Alarm &= PCF8563_Shield_WeekDays_Alarm;

    //判断需要的数据格式
    if (PCF_Format == PCF_Format_BIN)
    {
        PCF_DataStruct->Years = RTC_Bcd2ToBin(PCF_DataStruct->Years);
        PCF_DataStruct->Months_Century = RTC_Bcd2ToBin(PCF_DataStruct->Months_Century);
        PCF_DataStruct->Days = RTC_Bcd2ToBin(PCF_DataStruct->Days);
        PCF_DataStruct->WeekDays = RTC_Bcd2ToBin(PCF_DataStruct->WeekDays);

        PCF_DataStruct->Hours = RTC_Bcd2ToBin(PCF_DataStruct->Hours);
        PCF_DataStruct->Minutes = RTC_Bcd2ToBin(PCF_DataStruct->Minutes);
        PCF_DataStruct->Seconds = RTC_Bcd2ToBin(PCF_DataStruct->Seconds);

        PCF_DataStruct->Day_Alarm = RTC_Bcd2ToBin(PCF_DataStruct->Day_Alarm);
        PCF_DataStruct->WeekDays_Alarm = RTC_Bcd2ToBin(PCF_DataStruct->WeekDays_Alarm);

        PCF_DataStruct->Hour_Alarm = RTC_Bcd2ToBin(PCF_DataStruct->Hour_Alarm);
        PCF_DataStruct->Minute_Alarm = RTC_Bcd2ToBin(PCF_DataStruct->Minute_Alarm);
    }
}

/**
   ============================================================================
                      #### 时间信息操作功能函数 ####
   ============================================================================
**/

/**
  *****************************************************************************
  * @Func   : PCF8563写入时间信息
  *
  * @Brief  : none
  *
  * @Input  : PCF_Format:     数据格式
  *                           = PCF_Format_BIN
  *                           = PCF_Format_BCD
  *           PCF_DataStruct: 时间结构指针
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_SetTime(unsigned char PCF_Format, _PCF8563_Time_Typedef *PCF_DataStruct)
{
    if (PCF_Format == PCF_Format_BIN) //十进制格式，需要转换一下
    {
        //判断数值是否在范围之内
        if (PCF_DataStruct->RTC_Hours > 23)
            PCF_DataStruct->RTC_Hours = 0; //恢复0小时
        if (PCF_DataStruct->RTC_Minutes > 59)
            PCF_DataStruct->RTC_Minutes = 0; //恢复0分钟
        if (PCF_DataStruct->RTC_Seconds > 59)
            PCF_DataStruct->RTC_Seconds = 0; //恢复0秒

        //需要转换一下
        PCF_DataStruct->RTC_Hours = RTC_BinToBcd2(PCF_DataStruct->RTC_Hours);
        PCF_DataStruct->RTC_Minutes = RTC_BinToBcd2(PCF_DataStruct->RTC_Minutes);
        PCF_DataStruct->RTC_Seconds = RTC_BinToBcd2(PCF_DataStruct->RTC_Seconds);
    }

    //拷贝数据
    buffer[0] = PCF_DataStruct->RTC_Seconds;
    buffer[1] = PCF_DataStruct->RTC_Minutes;
    buffer[2] = PCF_DataStruct->RTC_Hours;

    //写入数据到寄存器
    PCF8563_Write_nByte(PCF8563_Address_Seconds, 3, buffer);
}
/**
  *****************************************************************************
  * @Func   : PCF8563读取时间信息
  *
  * @Brief  : none
  *
  * @Input  : PCF_Format:     数据格式
  *                           = PCF_Format_BIN
  *                           = PCF_Format_BCD
  *
  * @Output : PCF_DataStruct: 时间结构指针
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_GetTime(unsigned char PCF_Format, _PCF8563_Time_Typedef *PCF_DataStruct)
{
    rt_memset(buffer, 0, sizeof(buffer));
    //读取寄存器数值
    PCF8563_Read_nByte(PCF8563_Address_Seconds, 3, buffer);

    //屏蔽无效位
    buffer[0] &= PCF8563_Shield_Seconds;
    buffer[1] &= PCF8563_Shield_Minutes;
    buffer[2] &= PCF8563_Shield_Hours;

    //判断需要的数据格式
    if (PCF_Format == PCF_Format_BIN)
    {
        PCF_DataStruct->RTC_Hours = RTC_Bcd2ToBin(buffer[2]);
        PCF_DataStruct->RTC_Minutes = RTC_Bcd2ToBin(buffer[1]);
        PCF_DataStruct->RTC_Seconds = RTC_Bcd2ToBin(buffer[0]);
    }
    else if (PCF_Format == PCF_Format_BCD)
    {
        //拷贝数据
        PCF_DataStruct->RTC_Hours = buffer[2];
        PCF_DataStruct->RTC_Minutes = buffer[1];
        PCF_DataStruct->RTC_Seconds = buffer[0];
    }
}

/**
   ============================================================================
                         #### 日期信息操作功能函数 ####
   ============================================================================
**/

/**
  *****************************************************************************
  * @Func   : PCF8563写入日期信息
  *
  * @Brief  : 星期数值范围是: 0 ~ 6，十进制格式
  *
  * @Input  : PCF_Format:     数据格式
  *                           = PCF_Format_BIN
  *                           = PCF_Format_BCD
  *           PCF_Century:    世纪位设定
  *                           = PCF_Century_19xx
  *                           = PCF_Century_20xx
  *           PCF_DataStruct: 日期结构指针
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_SetDate(unsigned char PCF_Format, _PCF8563_Date_Typedef *PCF_DataStruct)
{
    if (PCF_Format == PCF_Format_BIN) //十进制格式，需要转换一下
    {
        //判断数值是否在范围之内
        if (PCF_DataStruct->RTC_Years > 99)
            PCF_DataStruct->RTC_Years = 0; //恢复00年
        if (PCF_DataStruct->RTC_Months > 12)
            PCF_DataStruct->RTC_Months = 1; //恢复1月
        if (PCF_DataStruct->RTC_Days > 31)
            PCF_DataStruct->RTC_Days = 1; //恢复1日
        if (PCF_DataStruct->RTC_WeekDays > 6)
            PCF_DataStruct->RTC_WeekDays = 1; //恢复星期一

        //需要转换一下
        PCF_DataStruct->RTC_Years = RTC_BinToBcd2(PCF_DataStruct->RTC_Years);
        PCF_DataStruct->RTC_Months = RTC_BinToBcd2(PCF_DataStruct->RTC_Months);
        PCF_DataStruct->RTC_Days = RTC_BinToBcd2(PCF_DataStruct->RTC_Days);
        PCF_DataStruct->RTC_WeekDays = RTC_BinToBcd2(PCF_DataStruct->RTC_WeekDays);
    }

    //数据拷贝
    buffer[0] = PCF_DataStruct->RTC_Days;
    buffer[1] = PCF_DataStruct->RTC_WeekDays;
    buffer[2] = PCF_DataStruct->RTC_Months;
    buffer[3] = PCF_DataStruct->RTC_Years;

    //写入数据到寄存器
    PCF8563_Write_nByte(PCF8563_Address_Days, 4, buffer);
}
/**
  *****************************************************************************
  * @Func   : PCF8563读取日期信息
  *
  * @Brief  : 星期数值范围是: 0 ~ 6，十进制格式
  *
  * @Input  : PCF_Format:     数据格式
  *                           = PCF_Format_BIN
  *                           = PCF_Format_BCD
  *
  * @Output : *PCF_Century:   世纪位，0：21世纪；1:20世纪
  *           PCF_DataStruct: 日期结构指针
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_GetDate(unsigned char PCF_Format, _PCF8563_Date_Typedef *PCF_DataStruct)
{
    //读取全部寄存器数值
    PCF8563_Read_nByte(PCF8563_Address_Days, 4, buffer);

    //屏蔽无效位
    buffer[0] &= PCF8563_Shield_Days;
    buffer[1] &= PCF8563_Shield_WeekDays;
    buffer[2] &= PCF8563_Shield_Months_Century;
    buffer[3] &= PCF8563_Shield_Years;

    //判断需要的数据格式
    if (PCF_Format == PCF_Format_BIN)
    {
        PCF_DataStruct->RTC_Years = RTC_Bcd2ToBin(buffer[3]);
        PCF_DataStruct->RTC_Months = RTC_Bcd2ToBin(buffer[2]);
        PCF_DataStruct->RTC_WeekDays = RTC_Bcd2ToBin(buffer[1]);
        PCF_DataStruct->RTC_Days = RTC_Bcd2ToBin(buffer[0]);
    }
    else if (PCF_Format == PCF_Format_BCD)
    {
        //拷贝数据
        PCF_DataStruct->RTC_Years = buffer[3];
        PCF_DataStruct->RTC_Months = buffer[2];
        PCF_DataStruct->RTC_WeekDays = buffer[1];
        PCF_DataStruct->RTC_Days = buffer[0];
    }
}

/**
   ============================================================================
                         #### 闹铃信息操作功能函数 ####
   ============================================================================
**/

/**
  *****************************************************************************
  * @Func   : PCF8563写入闹铃信息
  *
  * @Brief  : 星期数值范围是: 0 ~ 6，十进制格式
  *
  * @Input  : PCF_Format:     数据格式
  *                           = PCF_Format_BIN
  *                           = PCF_Format_BCD
  *           PCF_DataStruct: 闹铃结构指针
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_SetAlarm(unsigned char PCF_Format, _PCF8563_Alarm_Typedef *PCF_DataStruct)
{
    unsigned char Alarm_State = 0;
    unsigned char Alarm_Interrupt = 0; //控制/状态寄存器闹铃中断缓存

    if (PCF_Format == PCF_Format_BIN) //十进制格式，需要转换一下
    {
        //判断数值是否在范围之内
        if (PCF_DataStruct->RTC_AlarmMinutes > 59)
            PCF_DataStruct->RTC_AlarmMinutes = 0; //恢复0分钟
        if (PCF_DataStruct->RTC_AlarmHours > 23)
            PCF_DataStruct->RTC_AlarmHours = 0; //恢复0小时
        if (PCF_DataStruct->RTC_AlarmDays > 31)
            PCF_DataStruct->RTC_AlarmDays = 1; //恢复1日
        if (PCF_DataStruct->RTC_AlarmWeekDays > 6)
            PCF_DataStruct->RTC_AlarmWeekDays = 1; //恢复星期一

        //需要转换一下
        PCF_DataStruct->RTC_AlarmMinutes = RTC_BinToBcd2(PCF_DataStruct->RTC_AlarmMinutes);
        PCF_DataStruct->RTC_AlarmHours = RTC_BinToBcd2(PCF_DataStruct->RTC_AlarmHours);
        PCF_DataStruct->RTC_AlarmDays = RTC_BinToBcd2(PCF_DataStruct->RTC_AlarmDays);
        PCF_DataStruct->RTC_AlarmWeekDays = RTC_BinToBcd2(PCF_DataStruct->RTC_AlarmWeekDays);
    }

    //判断是否开启闹铃选择
    if (PCF_DataStruct->RTC_AlarmNewState == RTC_AlarmNewState_Open) //只打开闹铃
    {
        Alarm_State = 1;
    }
    else if (PCF_DataStruct->RTC_AlarmNewState == RTC_AlarmNewState_Open_INT_Enable) //打开闹铃并打开中断输出
    {
        Alarm_State = 2;
    }
    else if (PCF_DataStruct->RTC_AlarmNewState == RTC_AlarmNewState_Close) //关闭闹铃，并且关闭中断输出
    {
        Alarm_State = 3;
    }

    //读取控制/状态寄存器2值
    Alarm_Interrupt = PCF8563_Read_Byte(PCF8563_Address_Control_Status_2);
    Alarm_Interrupt &= PCF_Alarm_INT_Close; //先关闭中断输出
    Alarm_Interrupt &= PCF_Control_ClearAF;
    ; //清除标志
    PCF8563_Write_Byte(PCF8563_Address_Control_Status_2, Alarm_Interrupt);

    //根据开启类型进行相应操作
    if (Alarm_State == 1 || Alarm_State == 2) //打开闹铃
    {
        if (PCF_DataStruct->RTC_AlarmType & RTC_AlarmType_Days)
            PCF_DataStruct->RTC_AlarmDays &= PCF_Alarm_DaysOpen; //日期闹铃
        if (PCF_DataStruct->RTC_AlarmType & RTC_AlarmType_Hours)
            PCF_DataStruct->RTC_AlarmHours &= PCF_Alarm_HoursOpen; //小时闹铃
        if (PCF_DataStruct->RTC_AlarmType & RTC_AlarmType_Minutes)
            PCF_DataStruct->RTC_AlarmMinutes &= PCF_Alarm_MinutesOpen; //分钟闹铃
        if (PCF_DataStruct->RTC_AlarmType & RTC_AlarmType_WeekDays)
            PCF_DataStruct->RTC_AlarmWeekDays &= PCF_Alarm_WeekDaysOpen; //分钟闹铃
    }
    if (Alarm_State == 3) //关闭
    {
        if (PCF_DataStruct->RTC_AlarmType & RTC_AlarmType_Days)
            PCF_DataStruct->RTC_AlarmDays |= PCF_Alarm_DaysClose; //日期闹铃
        if (PCF_DataStruct->RTC_AlarmType & RTC_AlarmType_Hours)
            PCF_DataStruct->RTC_AlarmHours |= PCF_Alarm_HoursClose; //小时闹铃
        if (PCF_DataStruct->RTC_AlarmType & RTC_AlarmType_Minutes)
            PCF_DataStruct->RTC_AlarmMinutes |= PCF_Alarm_MinutesClose; //分钟闹铃
        if (PCF_DataStruct->RTC_AlarmType & RTC_AlarmType_WeekDays)
            PCF_DataStruct->RTC_AlarmWeekDays |= PCF_Alarm_WeekDaysClose; //分钟闹铃
    }

    //判断是否开启中断输出
    if (Alarm_State == 2)
    {
        Alarm_Interrupt |= PCF_Alarm_INT_Open;
        Alarm_Interrupt &= PCF_Control_ClearAF;
        ; //清除标志
    }

    //拷贝数据
    buffer[0] = PCF_DataStruct->RTC_AlarmMinutes;
    buffer[1] = PCF_DataStruct->RTC_AlarmHours;
    buffer[2] = PCF_DataStruct->RTC_AlarmDays;
    buffer[3] = PCF_DataStruct->RTC_AlarmWeekDays;

    //写入数据到寄存器
    PCF8563_Write_nByte(PCF8563_Alarm_Minutes, 4, buffer);

    //写入控制/状态寄存器2数值
    PCF8563_Write_Byte(PCF8563_Address_Control_Status_2, Alarm_Interrupt);
}
/**
  *****************************************************************************
  * @Func   : PCF8563读取闹铃信息
  *
  * @Brief  : 星期数值范围是: 0 ~ 6，十进制格式；只是返回了闹铃寄存器数值，开关位、中断输出什么的不返回
  *
  * @Input  : PCF_Format:     数据格式
  *                           = PCF_Format_BIN
  *                           = PCF_Format_BCD
  *
  * @Output : PCF_DataStruct: 闹铃结构指针
  *
  * @Return : none
  *****************************************************************************
**/
void PCF8563_GetAlarm(unsigned char PCF_Format, _PCF8563_Alarm_Typedef *PCF_DataStruct)
{
    //读取全部寄存器数值
    PCF8563_Read_nByte(PCF8563_Alarm_Minutes, 4, buffer);

    //屏蔽无效位，分钟报警值全部返回
    buffer[0] &= PCF8563_Shield_Minute_Alarm;   //分钟报警值
    buffer[1] &= PCF8563_Shield_Hour_Alarm;     //小时报警值
    buffer[2] &= PCF8563_Shield_Day_Alarm;      //日期报警值
    buffer[3] &= PCF8563_Shield_WeekDays_Alarm; //星期报警值

    //判断需要的数据格式
    if (PCF_Format == PCF_Format_BIN)
    {
        PCF_DataStruct->RTC_AlarmDays = RTC_Bcd2ToBin(buffer[2]);
        PCF_DataStruct->RTC_AlarmHours = RTC_Bcd2ToBin(buffer[1]);
        PCF_DataStruct->RTC_AlarmMinutes = RTC_Bcd2ToBin(buffer[0]);
        PCF_DataStruct->RTC_AlarmWeekDays = RTC_Bcd2ToBin(buffer[3]);
    }
    else if (PCF_Format == PCF_Format_BCD)
    {
        //拷贝数据
        PCF_DataStruct->RTC_AlarmDays = buffer[2];
        PCF_DataStruct->RTC_AlarmHours = buffer[1];
        PCF_DataStruct->RTC_AlarmMinutes = buffer[0];
        PCF_DataStruct->RTC_AlarmWeekDays = buffer[3];
    }
}
#include "time.h"
#include <stdio.h>
/**read time**/
int pcf8563_GetCounter(void)
{
    _PCF8563_Register_Typedef PCF8563_Register;
    PCF8563_GetRegister(PCF_Format_BIN, &PCF8563_Register);

    struct tm ti;
    ti.tm_year = PCF8563_Register.Years + 2000 - 1900;
    ti.tm_mon = PCF8563_Register.Months_Century - 1;
    ti.tm_mday = PCF8563_Register.Days;
    ti.tm_hour = PCF8563_Register.Hours;
    ti.tm_min = PCF8563_Register.Minutes;
    ti.tm_sec = PCF8563_Register.Seconds;
    time_t now;
    now = mktime(&ti);

    return now;
}

int pcf8563_SetCounter(rt_uint32_t now)
{
    struct tm *ti;
    _PCF8563_Register_Typedef PCF8563_Register;

    ti = localtime((const time_t *)&now);
    PCF8563_Register.Years = (ti->tm_year + 1900) % 100;
    PCF8563_Register.Months_Century = ti->tm_mon + 1;
    PCF8563_Register.WeekDays = ti->tm_wday;
    PCF8563_Register.Days = ti->tm_mday;
    PCF8563_Register.Hours = ti->tm_hour;
    PCF8563_Register.Minutes = ti->tm_min;
    PCF8563_Register.Seconds = ti->tm_sec;

    PCF8563_Register.Years = RTC_BinToBcd2(PCF8563_Register.Years);
    PCF8563_Register.Months_Century = RTC_BinToBcd2(PCF8563_Register.Months_Century);
    PCF8563_Register.WeekDays = RTC_BinToBcd2(PCF8563_Register.WeekDays);
    PCF8563_Register.Days = RTC_BinToBcd2(PCF8563_Register.Days);
    PCF8563_Register.Hours = RTC_BinToBcd2(PCF8563_Register.Hours);
    PCF8563_Register.Minutes = RTC_BinToBcd2(PCF8563_Register.Minutes);
    PCF8563_Register.Seconds = RTC_BinToBcd2(PCF8563_Register.Seconds);

    PCF8563_SetMode(PCF_Mode_Normal); //设置模式
    PCF8563_Stop();                   //PCF8563停止
    PCF8563_Write_nByte(PCF8563_Address_Seconds, 7, &PCF8563_Register.Seconds);
    PCF8563_Start();
    return 0;
}

/************************************************ END OF FILE ************************************************/
