
#include <stdint.h>
#include <stdbool.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_i2c.h"
#include "hw_types.h"
#include "i2c.h"
#include "pin_map.h"
#include "sysctl.h"
#include "SysTick.h"
#include "interrupt.h"
#include "hw_ints.h"

//*****************************************************************************
//
//I2C GPIO chip address and resigster define
//
//*****************************************************************************
#define TCA6424_I2CADDR 					0x22
#define PCA9557_I2CADDR						0x18

#define PCA9557_INPUT							0x00
#define	PCA9557_OUTPUT						0x01
#define PCA9557_POLINVERT					0x02
#define PCA9557_CONFIG						0x03

#define TCA6424_CONFIG_PORT0			0x0c
#define TCA6424_CONFIG_PORT1			0x0d
#define TCA6424_CONFIG_PORT2			0x0e

#define TCA6424_INPUT_PORT0				0x00
#define TCA6424_INPUT_PORT1				0x01
#define TCA6424_INPUT_PORT2				0x02

#define TCA6424_OUTPUT_PORT0			0x04
#define TCA6424_OUTPUT_PORT1			0x05
#define TCA6424_OUTPUT_PORT2			0x06

#define SYSTICK_FREQUENCY					1000			//1000hz

#define	I2C_FLASHTIME						500				//500ms，I2C定时时长
#define GPIO_FLASHTIME						50				//50ms，GPIO定时时长


void 		S800_Clock_Init(void);
void 		S800_GPIO_Init(void);
void		S800_I2C0_Init(void);
void 		Delay(uint32_t value);
uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData);
uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr);
void 		S800_SysTick_Init(void);

uint32_t ui32SysClock;
uint32_t flashtime = I2C_FLASHTIME;
uint32_t min = 0, sec = 0;
uint32_t key_value_1 = 0, key_value_2 = 0;
uint8_t seg7[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x77,0x7c,0x58,0x5e,0x079,0x71,0x5c};

//systick software counter define
volatile uint16_t systick_10ms_couter=0, systick_200ms_couter=0; //10ms和200ms计时器
volatile uint8_t	systick_10ms_status=0, systick_200ms_status=0; //10ms和200ms计时状态
volatile uint16_t i2c_flash_cnt=0, gpio_flash_cnt=0;

int main(void)
{
	volatile uint8_t result;
	volatile uint8_t cnt=0; 
	volatile uint8_t cnt1=0; 
	uint8_t show[5], i;
	
	IntMasterDisable();	//关中断

	S800_Clock_Init();
	S800_GPIO_Init();
	S800_I2C0_Init();
	S800_SysTick_Init();

	IntMasterEnable();	//开中断
	
	while (1)
	{
		key_value_1 = GPIOPinRead(GPIO_PORTJ_BASE,GPIO_PIN_0);
		key_value_2 = GPIOPinRead(GPIO_PORTJ_BASE,GPIO_PIN_1);
		
		if (systick_200ms_status) //200ms定时到
		{
			systick_200ms_status = 0; //重置200ms定时状态
			//如果判断到对应按键按下，对应值+1
			//控制秒数时，秒数不再受正常的1s影响
			if (key_value_1 == 0) sec++,i2c_flash_cnt = 0;
			if (key_value_2 == 0) min++;
			//正常情况，1s跳变一次
			if (++i2c_flash_cnt	>= 5)  //5*200ms=1s
			{
				i2c_flash_cnt = 0;
				sec++;
			}
		}
		if (sec >= 60) min++, sec -= 60;
		if (min >= 60) min -= 60;
		// 将显示分钟、秒钟需要的值写入数组
		show[0] = seg7[min/10];
		show[1] = seg7[min%10];
		show[2] = 0x40; // '-'
		show[3] = seg7[sec/10];
		show[4] = seg7[sec%10];
		//数码管显示
		for(i = 0; i < 5; i++)
		{
			result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,0); //防拖影
			result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT1,show[i]);
			result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,1<<i);
			SysCtlDelay(ui32SysClock/3000); //延时1ms
		}
	}

}

void S800_Clock_Init(void)
{
	//use internal 16M oscillator, HSI
	//ui32SysClock = SysCtlClockFreqSet((SYSCTL_OSC_INT |SYSCTL_USE_OSC), 16000000);		

	//use extern 25M crystal
	//ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN |SYSCTL_USE_OSC), 25000000);		

	//use PLL
	ui32SysClock = SysCtlClockFreqSet((SYSCTL_OSC_INT | SYSCTL_USE_PLL |SYSCTL_CFG_VCO_480), 20000000);
}

void S800_GPIO_Init(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);						//Enable PortF
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));			//Wait for the GPIO moduleF ready
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);						//Enable PortJ	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ));			//Wait for the GPIO moduleJ ready	
	
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0);			//Set PF0 as Output pin
	GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE,GPIO_PIN_0 | GPIO_PIN_1);//Set the PJ0,PJ1 as input pin
	GPIOPadConfigSet(GPIO_PORTJ_BASE,GPIO_PIN_0 | GPIO_PIN_1,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPU);
	
	// 设置PJ0和PJ1下降沿触发	
	GPIOIntTypeSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_FALLING_EDGE);
	// 使能PJ0和PJ1引脚中断
	GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	// 使能PORTJ中断
	IntEnable(INT_GPIOJ);
	
}

void S800_I2C0_Init(void)
{
	uint8_t result;
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	GPIOPinConfigure(GPIO_PB2_I2C0SCL);
	GPIOPinConfigure(GPIO_PB3_I2C0SDA);
	GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
	GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

	I2CMasterInitExpClk(I2C0_BASE,ui32SysClock, true);										//config I2C0 400k
	I2CMasterEnable(I2C0_BASE);	

	result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_CONFIG_PORT0,0x0ff);		//config port 0 as input
	result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_CONFIG_PORT1,0x0);			//config port 1 as output
	result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_CONFIG_PORT2,0x0);			//config port 2 as output 

	result = I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_CONFIG,0x00);				//config port as output
	result = I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,0x0ff);				//turn off the LED1-8
	
}

uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData)
{
	uint8_t rop;
	
	while(I2CMasterBusy(I2C0_BASE)){}; //忙等待
	I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false); //设从机地址，写
	I2CMasterDataPut(I2C0_BASE, RegAddr);	//设数据地址
	I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START); //启动总线发送
	while(I2CMasterBusy(I2C0_BASE)){};
	rop = (uint8_t)I2CMasterErr(I2C0_BASE);

	I2CMasterDataPut(I2C0_BASE, WriteData); //设数据值
	I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH); //启动总线发送，发送后停止
	while(I2CMasterBusy(I2C0_BASE)){};
	rop = (uint8_t)I2CMasterErr(I2C0_BASE);

	return rop;
}

uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr)
{
	uint8_t value;
	
	while(I2CMasterBusy(I2C0_BASE)){};	//忙等待
	I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false); //设从机地址，写
	I2CMasterDataPut(I2C0_BASE, RegAddr); //设数据地址
	I2CMasterControl(I2C0_BASE,I2C_MASTER_CMD_SINGLE_SEND); //启动总线发送
	while(I2CMasterBusBusy(I2C0_BASE));
	if (I2CMasterErr(I2C0_BASE) != I2C_MASTER_ERR_NONE)
		return 0; //错误
	Delay(100);

	//receive data
	I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, true); //设从机地址，读
	I2CMasterControl(I2C0_BASE,I2C_MASTER_CMD_SINGLE_RECEIVE); //启动总线接收
	while(I2CMasterBusBusy(I2C0_BASE));
	if (I2CMasterErr(I2C0_BASE) != I2C_MASTER_ERR_NONE)
		return 0; //错误
	Delay(100);

	value=I2CMasterDataGet(I2C0_BASE);

	return value;
}

void Delay(uint32_t value)
{
	uint32_t ui32Loop;
	
	for(ui32Loop = 0; ui32Loop < value; ui32Loop++){};
}

void S800_SysTick_Init(void)
{
	SysTickPeriodSet(ui32SysClock/SYSTICK_FREQUENCY); //定时1ms
	SysTickEnable();
	SysTickIntEnable();
}

/*
	Corresponding to the startup_TM4C129.s vector table systick interrupt program name
*/
void SysTick_Handler(void)
{
	if (systick_200ms_couter == 0) //利用1ms的SysTick产生200ms的定时器
	{
		systick_200ms_couter = 200;
		systick_200ms_status = 1;
	}
	else
		systick_200ms_couter--;
	
	if (systick_10ms_couter	== 0) //利用1ms的SysTick产生10ms的定时器
	{
		systick_10ms_couter	 = 10;
		systick_10ms_status  = 1;
	}
	else
		systick_10ms_couter--;
}

void GPIOJ_Handler(void)
{
	unsigned long intStatus;
	
	intStatus = GPIOIntStatus(GPIO_PORTJ_BASE, true);
	
	GPIOIntClear(GPIO_PORTJ_BASE, intStatus);

	if (intStatus & GPIO_PIN_0)
	{
		sec++;
		i2c_flash_cnt = 0;
		systick_200ms_couter = 200; // 重新计数
	}
	if (intStatus & GPIO_PIN_1)
	{
		min++;
		i2c_flash_cnt = 0;
		systick_200ms_couter = 200; // 重新计数
	}
	SysCtlDelay( ui32SysClock / 100); // 用于消抖
}

