#include <stdint.h>
#include <stdbool.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_i2c.h"
#include "hw_types.h"
#include "pin_map.h"
#include "sysctl.h"
#include "sysTick.h"
#include "interrupt.h"
#include "hw_ints.h"

void S800_Clock_Init(void);
void S800_GPIO_Init(void);
void S800_SysTick_Init(void);

uint32_t ui32SysClock;
uint8_t  btn_cnt = 0;

//systick software counter define
volatile uint8_t systick_1s_status=0, systick_500ms_status=0;

int main(void)
{
	IntMasterDisable();

	S800_Clock_Init();
	S800_GPIO_Init();
	S800_SysTick_Init(); 

	IntMasterEnable();

	while (1)
	{	
		if (systick_500ms_status && btn_cnt == 1) {
			//第1次按USR_SW1,LED_M0以1s的周期闪烁
			//当systick_500ms_status=1时，即经过500ms，LED_M0状态按位取反
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);
			GPIOPinWrite( GPIO_PORTF_BASE, GPIO_PIN_0, ~GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0));
			systick_500ms_status = 0;
		}

		if (systick_1s_status && btn_cnt == 3) {
			//第3次按USR_SW1,LED_M1以2s的周期闪烁
			//当systick_1s_status=1时，即经过1s，LED_M1状态按位取反
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
			GPIOPinWrite( GPIO_PORTF_BASE, GPIO_PIN_1, ~GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));
			systick_1s_status = 0;
		}
		
		if (btn_cnt % 2 == 0)
		{
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);
		}
	}
	
}

void S800_Clock_Init(void)
{
	//use internal 16M oscillator, HSI
	ui32SysClock = SysCtlClockFreqSet((SYSCTL_OSC_INT |SYSCTL_USE_OSC), 16000000);		

	//use extern 25M crystal
	//ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN |SYSCTL_USE_OSC), 2500000);		

	//use PLL
	//ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN |SYSCTL_USE_PLL |SYSCTL_CFG_VCO_480), 60000000);	
}

void S800_GPIO_Init(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);						//Enable PortF
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));			//Wait for the GPIO moduleF ready

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);						//Enable PortJ	
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ));			//Wait for the GPIO moduleJ ready	
	
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1);	//Set PF0,PF1 as Output pin

	GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0);		//Set the PJ0 as input pin
	GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

	 //设置PJ0下降沿触发
	GPIOIntTypeSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_FALLING_EDGE);
	 // 使能PJ0引脚中断
	GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_0);
	 // 使能PORTJ中断
	IntEnable(INT_GPIOJ);
}
	
void GPIOJ_Handler(void)
{
	unsigned long intStatus;

	intStatus = GPIOIntStatus(GPIO_PORTJ_BASE, true);
	GPIOIntClear(GPIO_PORTJ_BASE, intStatus);

	if (intStatus & GPIO_PIN_0) {
	     btn_cnt = btn_cnt % 4 + 1;
	}
	
	//systick_500ms_status = 1;
	//systick_1s_status = 1;
	
	SysCtlDelay( ui32SysClock / 60);
}

void S800_SysTick_Init(void)
{
	SysTickPeriodSet(ui32SysClock/2); // 500ms
	SysTickEnable();
	SysTickIntEnable();
}

void SysTick_Handler(void)
{
	static uint8_t k = 0;	//定义静态变量k，实现多周期控制
	
	systick_500ms_status = 1;
	
	if (k) systick_1s_status = 1;

	k = 1 - k;
}
