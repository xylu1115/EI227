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
#include "systick.h"
#include "interrupt.h"
#include "uart.h"
#include "hw_ints.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
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
uint8_t 	I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData);
uint8_t 	I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr);
void 		S800_SysTick_Init(void);
void 		S800_UART_Init(void);
void 		UARTStringPut(const char *cMessage);
void 		UARTStringPutNonBlocking(const char *cMessage);

uint32_t ui32SysClock;
uint32_t flashtime = I2C_FLASHTIME;
uint32_t min = 0, sec = 0, hour = 12;
uint8_t seg7[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x77,0x7c,0x58,0x5e,0x079,0x71,0x5c};

//systick software counter define
volatile uint16_t systick_1000ms_couter=1000;
volatile uint8_t  systick_1000ms_status=0;
volatile uint8_t  uart_receive_status = 0;

int main(void)
{
	volatile uint8_t result;
	uint8_t show[8], i;
	char uart_receive_char;
	char input_str[20] = {0}, output_str[20] = {0}, operation[5] = {0};
	int n = 0;
	int h, m, s;
	
	IntMasterDisable();	//关中断

	S800_Clock_Init();
	S800_GPIO_Init();
	S800_I2C0_Init();
	S800_SysTick_Init();
	S800_UART_Init();

	IntMasterEnable();	//开中断
	
	while (1)
	{
		if (systick_1000ms_status) //1000ms定时到
		{
			systick_1000ms_status = 0; //重置1000ms定时状态
			sec++;
		}
		
		if (uart_receive_status)
		{
			n = 0;
			while(UARTCharsAvail(UART0_BASE))    // Loop while there are characters in the receive FIFO.
			{
				//Read the next character from the UART and write it back to the UART.
				uart_receive_char = UARTCharGetNonBlocking(UART0_BASE);
				if (uart_receive_char <= 'z' && uart_receive_char >= 'a')
					uart_receive_char -= 'a' - 'A';
				if (uart_receive_char == '-') uart_receive_char = ':';
				input_str[n++] = uart_receive_char;
			}
			input_str[n] = 0;
			if (strncmp(input_str, "GETTIME", 7)==0)
				//直接组装时、分、秒
				sprintf(output_str, "TIME%02d:%02d:%02d", hour, min, sec);
			else
			{
				sscanf(input_str, "%3s%d:%d:%d", operation, &h, &m, &s);
				//分割后判断是什么操作，并进行对应操作
				if (strncmp(operation, "SET", 3) == 0)
					hour = h, min = m, sec = s;
				else if (strncmp(operation, "INC", 3) == 0)
					hour += h, min += m, sec += s;
				if (sec >= 60) min++, sec -= 60;
				if (min >= 60) min -= 60, hour++;
				if (hour >= 24) hour -= 24;
				sprintf(output_str, "TIME%02d:%02d:%02d", hour, min, sec);
			}
			UARTStringPut(output_str);
			UARTStringPut("\r\n");

			uart_receive_status = 0;
		}
		
		if (sec >= 60) min++, sec -= 60;
		if (min >= 60) min -= 60, hour++;
		if (hour >= 24) hour -= 24;
		// 将显示小时、分钟、秒钟需要的值写入数组
		show[0] = seg7[hour/10];
		show[1] = seg7[hour%10];
		show[2] = 0x40; // '-'
		show[3] = seg7[min/10];
		show[4] = seg7[min%10];
		show[5] = 0x40;
		show[6] = seg7[sec/10];
		show[7] = seg7[sec%10];
		//数码管显示
		for(i = 0; i < 8; i++)
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
	if (systick_1000ms_couter == 0) //利用1ms的SysTick产生1s的定时器
	{
		systick_1000ms_couter = 1000;
		systick_1000ms_status = 1;
	}
	else
		systick_1000ms_couter--;
}

//----------- UART ---------------------
void S800_UART_Init(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);						//Enable PortA
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));			//Wait for the GPIO moduleA ready

	GPIOPinConfigure(GPIO_PA0_U0RX);												// Set GPIO A0 and A1 as UART pins.
	GPIOPinConfigure(GPIO_PA1_U0TX);    			

	GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

	// Configure the UART for 115,200, 8-N-1 operation.
	UARTConfigSetExpClk(UART0_BASE, ui32SysClock,115200,(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |UART_CONFIG_PAR_NONE));

	//UARTFIFOLevelSet(UART0_BASE,UART_FIFO_TX2_8,UART_FIFO_RX4_8);//set FIFO Level
	UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX4_8, UART_FIFO_RX7_8);
	UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);	//Enable UART0 RX,TX interrupt
	IntEnable(INT_UART0);
	
	//UARTStringPut("\r\nHello, world!\r\n");
}

void UARTStringPut(const char *cMessage)
{
	while(*cMessage!='\0')
		UARTCharPut(UART0_BASE,*(cMessage++));
}

void UARTStringPutNonBlocking(const char *cMessage)
{
	while(*cMessage!='\0')
		UARTCharPutNonBlocking(UART0_BASE,*(cMessage++));
}

/*
	Corresponding to the startup_TM4C129.s vector table UART0_Handler interrupt program name
*/
void UART0_Handler(void)
{
	int32_t uart0_int_status;
	//char uart_receive_char;
	
	uart0_int_status = UARTIntStatus(UART0_BASE, true);			// Get the interrrupt status.
	UARTIntClear(UART0_BASE, uart0_int_status);							//Clear the asserted interrupts

	if (uart0_int_status & (UART_INT_RX | UART_INT_RT)) 	//接收或接收超时
	{
		uart_receive_status = 1;
	}
}

