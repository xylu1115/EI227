#include <stdint.h>
#include <stdbool.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_types.h"
#include "pin_map.h"
#include "sysctl.h"
#include "interrupt.h"
#include "hw_ints.h"

#define   FASTFLASHTIME			(uint32_t)500000
#define   SLOWFLASHTIME			(uint32_t)2000000

//全局变量
uint32_t ui32SysClock;
uint32_t state = 0;

void S800_Clock_Init(void);
void S800_GPIO_Init(void);
void Delay(uint32_t value);


int main(void)
{
	uint32_t delay_time = SLOWFLASHTIME;
	
	S800_Clock_Init();
	S800_GPIO_Init();
	IntMasterEnable();	//使能处理器中断
	
	while(1)
	{
		if (state%2 == 0)
		{
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);
		}
		else if (state == 1)
		{
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);			// Turn on the LED.
			Delay(delay_time);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);							// Turn off the LED.
			Delay(delay_time);
		}
		else if (state == 3)
		{
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);			// Turn on the LED.
			Delay(delay_time);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);							// Turn off the LED.
			Delay(delay_time);
		}
	}
}

void S800_Clock_Init(void)
{
	//use internal 16M oscillator, HSI
	ui32SysClock = SysCtlClockFreqSet((SYSCTL_OSC_INT |SYSCTL_USE_OSC), 16000000);		

	//use extern 25M crystal
	//ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN |SYSCTL_USE_OSC), 25000000);		

	//use PLL
	//ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN |SYSCTL_USE_PLL |SYSCTL_CFG_VCO_480), 60000000);	
}

void S800_GPIO_Init(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);						//Enable PortF
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));	//Wait for the GPIO moduleF ready

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);						//Enable PortJ	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ));	//Wait for the GPIO moduleJ ready	
	
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1);//Set PF0 as Output pin

	GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0);			//Set PJ0 as input pin
	GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	
	 // 设置PJ0下降沿触发
	GPIOIntTypeSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_FALLING_EDGE);
	 // 使能PJ0引脚中断
	GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_0);
	 // 使能PORTJ中断
	IntEnable(INT_GPIOJ);
}

void Delay(uint32_t value)
{
	uint32_t ui32Loop;
	
	for(ui32Loop = 0; ui32Loop < value; ui32Loop++){};
}

void GPIOJ_Handler(void)	//重写中断服务函数
{
	unsigned long intStatus;
	
	intStatus = GPIOIntStatus(GPIO_PORTJ_BASE, true);	//读取中断请求情况
	
	GPIOIntClear(GPIO_PORTJ_BASE, intStatus);	//清除中断请求

	if (intStatus & GPIO_PIN_0) 
	{
		state = (state + 1) % 4; //state全局变量
	}
	SysCtlDelay( ui32SysClock / 150);
}
