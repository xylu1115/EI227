#include <stdint.h>
#include <stdbool.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_types.h"
#include "pin_map.h"
#include "sysctl.h"


//全局变量
uint32_t ui32SysClock;

void S800_Clock_Init(void);
void S800_GPIO_Init(void);
void Delay(uint32_t value);


int main(void)
{
	int i = 0;
	S800_Clock_Init();
	S800_GPIO_Init();
	
	//模拟步进电机，PF3~0顺时针转，PF0~3逆时钟转
	while(1)
	{
		//顺时针
		for (i = 0;i < 512;i++)
		{		
			//八拍控制
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);	// D
			SysCtlDelay(5*ui32SysClock/3000); //延时5ms
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);	// DC
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x0);			// C
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);	// CB
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x0);			// B
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);	// BA
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);			// A
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);	// AD
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x0);
		}
		SysCtlDelay(ui32SysClock); // 暂停3秒
		
		//逆时针
		for (i = 0;i < 512;i++)
		{		
			//八拍
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);	// A
			SysCtlDelay(5*ui32SysClock/3000); //延时5ms
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);	// AB
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);			// B
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);	// BC
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);			// C
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);	// CD
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x0);			// D
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);	// DA
			SysCtlDelay(5*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x0);
		}
		SysCtlDelay(ui32SysClock); // 暂停3秒
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
	
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);			//Set PF0~3 as Output pin

	GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0);			//Set PJ0 as input pin
	GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void Delay(uint32_t value)
{
	uint32_t ui32Loop;
	
	for(ui32Loop = 0; ui32Loop < value; ui32Loop++){};
}
