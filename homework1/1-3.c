#include <stdint.h>
#include <stdbool.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_types.h"
#include "pin_map.h"
#include "sysctl.h"

#define   FASTFLASHTIME			(uint32_t)500000
#define   SLOWFLASHTIME			(uint32_t)1000000

//全局变量
uint32_t ui32SysClock;
uint32_t state = 0;

void S800_Clock_Init(void);
void S800_GPIO_Init(void);
void Delay(uint32_t value);


int main(void)
{
	uint32_t delay_time, key_value, last_key_value;
	
	S800_Clock_Init();
	S800_GPIO_Init();
	
	while(1)
	{
		last_key_value = key_value;			//记录上一次按键的值
		SysCtlDelay( ui32SysClock / 150);	//经过20ms后再读取，进行消抖
		
		key_value = GPIOPinRead(GPIO_PORTJ_BASE,GPIO_PIN_0);	//读取此时的按键值
		delay_time = SLOWFLASHTIME;
		if (key_value == 0 && last_key_value == 1)	//当判断按键按下后state状态更新
		{
			state = (state+1)%4;		//state用于记录按键次数
		}
		
		if (state%2 == 0)		//state = 0或2，熄灭两灯
		{
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);
		}
		else if (state == 1)	//state = 1，闪烁LED_M0
		{
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);
			Delay(delay_time);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
			Delay(delay_time);
		}
		else if (state == 3)	//state = 3，闪烁LED_M1
		{
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);
			Delay(delay_time);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);
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
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));			//Wait for the GPIO moduleF ready

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);						//Enable PortJ	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ));			//Wait for the GPIO moduleJ ready	
	
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1);			//Set PF0 as Output pin

	GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0);			//Set PJ0 as input pin
	GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void Delay(uint32_t value)
{
	uint32_t ui32Loop;
	
	for(ui32Loop = 0; ui32Loop < value; ui32Loop++){};
}
