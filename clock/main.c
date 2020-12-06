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
#include "adc.h"
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

#define	I2C_FLASHTIME						500				//
#define GPIO_FLASHTIME						50				//


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
void		S800_ADC_Init(void);

void reset(void); 							//系统重启
void showtime(void); 						//时间显示
void adjust(void); 							//小时、分、秒的调整
void showdate(void); 						//日期显示
void settime(void); 						//时间设置
void setdate(void); 						//日期设置
void clockshow(int t); 						//闹钟的显示和设置
void setparam(void); 						//根据按键调整参数
void ring(int t); 							//闹钟到时
void writetime(int *show); 					//辅助函数，将要显示的时间写入数组show
void rotate(int n, int state); 				//步进电机旋转n个步长
void writeseg(int n, int *show, bool flag); //辅助函数，将数组show的第n位写入数码管显示
void serialport(void); 						//串行口功能实现函数

uint32_t ui32SysClock;
uint8_t led = 0;							//LED控制变量
int student_num[8] = {2,1,9,1,0,4,1,6};
int min = 0, sec = 0, hour = 0, mon = 6, date = 21, year = 2020;	//当前时间
int clockhour[2] = {0}, clockmin[2] = {0}; 	//闹钟1、2计时时间
int angle_cnt[61], angle_now = 0;			//步进电机辅助变量
bool clock1able = 0, clock2able = 0, alarm_able = 1, night = 0;		//闹钟使能及夜晚记录
int func = 1;								//功能选择
int format = 24;							//12/24小时格式
uint8_t seg7[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x77,0x7c,0x58,0x5e,0x079,0x71,0x5c};
int month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

//systick software counter define
volatile uint16_t systick_1000ms_couter=1000, systick_5s_counter=5000, systick_500ms_counter=500, systick_250ms_counter=250;
volatile uint8_t  systick_1000ms_status=0, systick_5s_status=0, systick_500ms_status=0, systick_250ms_status=0;
volatile uint8_t  uart_receive_status = 0;

int main(void)
{
	volatile uint8_t result;
	int i = 0;
	//时间到i分时步进电机相对于0分需要转过的步长
	for(i = 0; i < 61; i++) angle_cnt[i] = (512.0/60.0)*i;	
	for(i = 0; i < 8; i++) student_num[i] = seg7[student_num[i]]; //转化格式
	
	IntMasterDisable();

	S800_Clock_Init();
	S800_GPIO_Init();
	S800_I2C0_Init();
	S800_SysTick_Init();
	S800_UART_Init();
	S800_ADC_Init();
	

	IntMasterEnable();
	reset();
	
	while (1)
	{
		serialport();	//串口功能
		adjust();		//时间调整
		setparam();		//按键调整参数
		
		if (angle_cnt[min]-angle_now!=0)	//调整步进电机指针
		{
			rotate((angle_cnt[min]-angle_now+512)%512, 2); //步长需要对512取模，2表示小角度旋转
			angle_now = angle_cnt[min];
		}
		
		//func控制功能选择，夜间熄灭失效
		if (func == 1 && !night) showtime();
		else if (func == 2 && !night) showdate();
		else if (func == 3 && !night) clockshow(1);
		else if (func == 4 && !night) clockshow(2);
		else if (func == 5 && !night) settime();
		else if (func == 6 && !night) setdate();
		
		if (night) //夜间熄灭
		{
			I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,0);
			I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,~0);
		}
		
		//时间到时，闹钟响铃
		while (hour == clockhour[0] && min == clockmin[0] && clock1able)
		{
			if (!alarm_able) break; //闹钟到时报警时按键使其失能
			ring(1);
		}
		while (hour == clockhour[1] && min == clockmin[1] && clock2able)
		{	
			if (!alarm_able) break;
			ring(2);
		}
		
		//时间未到，恢复报警使能
		if (!(hour == clockhour[0] && min == clockmin[0] && clock1able) 
			&& !(hour == clockhour[1] && min == clockmin[1] && clock2able))
			alarm_able = 1;
		
		//蜂鸣器静音
		GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, 0);
	}

}

//串口功能 //
void serialport()
{
	volatile uint8_t result;
	char uart_receive_char;
	char input_str[20] = {0}, output_str[20] = {0};
	int n = 0;
	uint32_t pui32ADC0Value[1];
	double ui32TempValueC;
	if (uart_receive_status)
	{
		n = 0;
		while(UARTCharsAvail(UART0_BASE))    // Loop while there are characters in the receive FIFO.
		{
			uart_receive_char = UARTCharGetNonBlocking(UART0_BASE); //接受数据
			if (uart_receive_char <= 'z' && uart_receive_char >= 'a')
				uart_receive_char -= 'a' - 'A';
			input_str[n++] = uart_receive_char; //写入数组
		}
		if (n>0) input_str[n] = '\0';
		if (strncmp(input_str, "GET+DATE", 8)==0) //获取日期
		{
			sprintf(output_str, "DATE%02d-%02d", mon, date);
			UARTStringPut(output_str); UARTStringPut("\r\n");
		}
		else if (strncmp(input_str, "GET+TIME", 8)==0) //获取时间
		{
			sprintf(output_str, "TIME%02d:%02d", hour, min);
			UARTStringPut(output_str); UARTStringPut("\r\n");
		}
		else if (strncmp(input_str, "RESET", 5)==0) //系统重启
			reset();
		else if (strncmp(input_str, "DSET", 4)==0) //设置日期
			sscanf(input_str, "DSET+%02d-%02d", &mon, &date);
		else if (strncmp(input_str, "TSET", 4)==0) //设置时间
			sscanf(input_str, "TSET+%02d:%02d", &hour, &min);
		else if (strncmp(input_str, "SET+ALM1", 8)==0) //设置闹钟1
			sscanf(input_str, "SET+ALM1+%02d:%02d", &clockhour[0], &clockmin[0]);
		else if (strncmp(input_str, "SET+ALM2", 8)==0) //设置闹钟2
			sscanf(input_str, "SET+ALM2+%02d:%02d", &clockhour[1], &clockmin[1]);
		else if (strncmp(input_str, "GET+TEMP", 8)==0) //获取芯片温度
		{
			ADCProcessorTrigger(ADC0_BASE, 3);
			while(!ADCIntStatus(ADC0_BASE, 3, false)) {}
			ADCIntClear(ADC0_BASE, 3);  //中断清除
			ADCSequenceDataGet(ADC0_BASE, 3, pui32ADC0Value); //获取ADC数据
			ui32TempValueC = 147.5 - (75 * 3.3 * pui32ADC0Value[0]) / 4096; //转换成温度
			sprintf(output_str, "%.2fC", ui32TempValueC); //格式化输出两位小数
			UARTStringPut(output_str); UARTStringPut("\r\n");
		}
				
		uart_receive_status = 0;
	}
	
	// 如果时间改变，调整步进电机
	if (angle_cnt[min]-angle_now!=0)
	{
		rotate((angle_cnt[min]-angle_now+512)%512, 1); //旋转的步长需要对512取模，1表示大角度旋转
		angle_now = angle_cnt[min];
	}
}

//将数组show的第n位写入数码管显示，flag用于在数码管闪烁中控制亮不亮 //
void writeseg(int n, int *show, bool flag)
{
	I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,0); //防拖影
	if (flag) I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT1,show[n]);
	if (flag) I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,1<<n);
	SysCtlDelay(ui32SysClock/3000);
	I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,0);
}

//步进电机旋转n个步长，state分为两个状态
//state=1表示大角度旋转，需要在旋转的过程中显示数码管
//state=2表示小角度旋转（瞬间完成），不需显示数码管
void rotate(int n, int state) //
{
	int i, show[8];
	//采用单四拍方式
	if (state == 2)
	{
		for(i = 0; i < n; i++)
		{
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);
			SysCtlDelay(2*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x0);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
			SysCtlDelay(2*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x0);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);
			SysCtlDelay(2*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);	
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);
			SysCtlDelay(2*ui32SysClock/3000);
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
		}
		return;
	}
	for(i = 0; i < n; i++)
	{
		writetime(show);
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);
		writeseg(0, show, 1);
		writeseg(1, show, 1);
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x0);
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
		writeseg(2, show, 1);
		writeseg(3, show, 1);
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x0);
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);
		writeseg(4, show, 1);
		writeseg(5, show, 1);		
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);	
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);
		writeseg(6, show, 1);
		writeseg(7, show, 1);			
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0);
	}
}

//将要显示的时间写入数组show
void writetime(int *show) //
{
	show[0] = seg7[hour/10];
	show[1] = seg7[hour%10];
	if (format == 12)
	{
		show[0] = seg7[(hour%12)/10];
		show[1] = seg7[(hour%12)%10];
	}
	show[2] = 0x40; // '-'
	show[3] = seg7[min/10];
	show[4] = seg7[min%10];
	show[5] = 0x40;
	show[6] = seg7[sec/10];
	show[7] = seg7[sec%10];
}

//闹钟到时，t表示闹钟1或2
void ring(int t)
{
	static int i;
	int	show[8];
	static bool flag = 0; //用于控制数码管闪烁
	night = 0; //闹钟到时，夜间模式关闭
	setparam();
	writetime(show);
	if (t==1)
	{
		//led翻转，实现1Hz闪烁，flag控制数码管闪烁
		//再与0x01取与是为了屏蔽掉除了第一位的其他位
		if (systick_500ms_status) led = (~led) & 0x01, systick_500ms_status = 0, flag = !flag;
		GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, ~GPIOPinRead(GPIO_PORTK_BASE, GPIO_PIN_5));
		SysCtlDelay(ui32SysClock/6000);
	}
	else if (t==2)
	{
		if (systick_250ms_status) led = (~led) & 0x02, systick_250ms_status = 0, flag = !flag;
		GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, ~GPIOPinRead(GPIO_PORTK_BASE, GPIO_PIN_5));
		SysCtlDelay(ui32SysClock/12000);
	}
	if (hour >= 12 && format == 12) led = led | 0x10; //指示PM
	I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,~led);
	
	//数码管闪烁
	I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,0);
	if (flag) {I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT1,show[i]); //显示
		I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,1<<i);}
	else {I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT1,0); //不显示
		I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,0);}
	i = (i+1)%8;
}

//按键调整
void setparam() //
{
	static uint8_t key_value = 0, last_value;
	last_value = key_value;
	key_value = ~I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
    if (hour < 21 && hour >= 8) night = 0; //白天自动点亮
	if (key_value == 0x01 && last_value == 0) func = (func % 6) + 1; //按键1按下
	if (key_value == 0x20 && last_value == 0) clock1able = !clock1able; //按键6按下，闹钟1使能/失能
	if (key_value == 0x40 && last_value == 0) clock2able = !clock2able; //按键7按下，闹钟2使能/失能
	if (key_value == 0x80 && last_value == 0) format = 36 - format; //按键8按下，12/24设置
	//有按键按下，闹钟报警时失能，同时重新计时5s
	if (key_value != 0x00 && key_value != 0xFF) alarm_able = 0, systick_5s_counter = 5000, systick_5s_status = 0;
	else if (systick_5s_status) func = 1, systick_5s_status = 0; //5s计时到，返回时间显示
}

//闹钟显示及设置 //
void clockshow(int t)
{
	int show[8], i, n;
	static uint8_t key_value, last_value;
	static bool sethour = 0, setmin = 0, flag = 0;
	//闹钟对应LED设置及时间写入
	led = 0x01*t;
	last_value = key_value;
	key_value = ~I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
	if (key_value == 0x02 && last_value == 0) sethour = 1, setmin = 0, clockhour[t-1]++;//SW2按下，小时设置
	if (key_value == 0x04 && last_value == 0) sethour = 0, setmin = 1, clockmin[t-1]++;//SW3按下，分钟设置
	adjust();
	n = clockhour[t-1]*100+clockmin[t-1];
	if (format == 12) n = (clockhour[t-1]%12)*100+clockmin[t-1];
	for (i = 4*t-1; i >= 4*(t-1); i--) //写入数组
	{
		show[i] = seg7[n%10];
		n /= 10;
	}
	if (systick_500ms_status) flag = !flag, systick_500ms_status = 0; //flag用于控制数码管闪烁
	if (sethour)
	{
		for(i = 4*(t-1); i < 4*t; i++)
			writeseg(i, show, (i < 2+4*(t-1) && flag) || i >= 2+4*(t-1)); //0、1数码管闪烁(t=1)，其余正常显示
	}
	else if (setmin)
	{
		for(i = 4*(t-1); i < 4*t; i++)
			writeseg(i, show, (i >= 2+4*(t-1) && flag) || i < 2+4*(t-1)); //2、3数码管闪烁(t=1)，其余正常显示
	}
	else
	{
		for(i = 4*(t-1); i < 4*t; i++)
			writeseg(i, show, 1); //未开始设置，正常显示
	}
	if (clockhour[t-1] >= 12 && format == 12) led = led | 0x10; //指示PM
	I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,~led);
}

//日期设置
void setdate() //
{
	int show[4], i, n;
	static uint8_t key_value, last_value;
	static bool setmon = 0, setdate = 0, flag = 0;
	last_value = key_value;
	key_value = ~I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
	if (key_value == 0x08 && last_value == 0) setmon = 1, setdate = 0, mon++;
	if (key_value == 0x10 && last_value == 0) setdate = 1, setmon = 0, date++;
	//日期写入
	n = mon*100+date;
	for (i = 3; i >= 0; i--)
	{
		show[i] = seg7[n%10];
		n /= 10;
	}
	adjust();
	//flag用于控制数码管闪烁，led设置第4位翻转
	if (systick_500ms_status) flag = !flag, systick_500ms_status = 0, led = (~led) & 0x08; 
	if (setmon)
	{
		for(i = 0; i < 4; i++)
			writeseg(i, show, (i < 2 && flag) || i >= 2);
	}
	else if (setdate)
	{
		for(i = 0; i < 4; i++)
			writeseg(i, show, (i >= 2 && flag) || i < 2);
	}
	else
	{
		for(i = 0; i < 4; i++)
			writeseg(i, show, 1);
	}
	I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,~led);
}

void settime() //
{
	int show[4], i, n;
	static uint8_t key_value, last_value;
	static bool sethour = 0, setmin = 0, flag = 0;
	last_value = key_value;
	key_value = ~I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
	if (key_value == 0x02 && last_value == 0) sethour = 1, setmin = 0, hour++;
	if (key_value == 0x04 && last_value == 0) sethour = 0, setmin = 1, min++;
	adjust();
	//步进电机小角度旋转
	if (angle_cnt[min]-angle_now!=0)
	{
		rotate((angle_cnt[min]-angle_now+512)%512, 2);
		angle_now = angle_cnt[min];
	}
	//显示的时间写入数组
	n = hour*100+min;
	if (format == 12) n = (hour%12)*100+min;
	for (i = 3; i >= 0; i--)
	{
		show[i] = seg7[n%10];
		n /= 10;
	}
	//flag用于控制数码管闪烁，led设置第3位翻转
	if (systick_500ms_status) flag = !flag, systick_500ms_status = 0, led = (~led) & 0x04;
	if (sethour)
	{
		for(i = 0; i < 4; i++)
			writeseg(i, show, (i < 2 && flag) || i >= 2);
	}
	else if (setmin)
	{
		for(i = 0; i < 4; i++)
			writeseg(i, show, (i >= 2 && flag) || i < 2);
	}
	else
	{
		for(i = 0; i < 4; i++)
			writeseg(i, show, 1);
	}
	if (hour >= 12 && format == 12) led = led | 0x10; //指示PM
	I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,~led);
}

//时间参数调整 //
void adjust()
{
	int i;
	if (sec >= 60) min++, sec -= 60;
	if (min >= 60) min -= 60, hour++;
	if (hour >= 24) hour -= 24, date++;
	if ((year%4==0&&year%100!=0)||year%400==0) month[1]=29; //闰年
	else month[1]=28;
	if (date > month[mon-1]) date = 1, mon++;
	if (mon > 12) mon = 1, year++;
	for(i = 0; i < 2; i++)
	{
		if (clockmin[i] >= 60) clockmin[i] -= 60, clockhour[i]++;
		if (clockhour[i] >= 24) clockhour[i] -= 24;
	}
}

//日期显示
void showdate() //
{
	int show[8], i;
	int n = year*10000+mon*100+date;
	if (hour >= 12 && format == 12) led = 0x10; //PM指示
	I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,~led);
	for (i = 7; i >= 0; i--) //日期写入数组
	{
		show[i] = seg7[n%10];
		n /= 10;
	}
	for(i = 0; i < 8; i++)
		writeseg(i, show, 1);
	I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,~led);
}

void showtime() //
{
	int show[8], i;
	writetime(show); //时间写入
	for(i = 0; i < 8; i++)
		writeseg(i, show, 1);
	if (systick_500ms_status) systick_500ms_status = 0, led = (~led) & 0x03;//LED1、2闪烁
	if (!clock1able) led = led & 0xFE; //闹钟无效时LED熄灭
	if (!clock2able) led = led & 0xFD;
	if (hour >= 12 && format == 12) led = led | 0x10; //PM指示
	I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,~led);
}

//重启
void reset(void) //
{
	int i = 0;
	min = 0, sec = 0, hour = 0, mon = 6, date = 21, year = 2020; //参数重置
	clockhour[0] = 0, clockmin[0] = 0, clockhour[1] = 0, clockmin[1] = 0;
	clock1able = 0, clock2able = 0, night = 0;
	func = 1, format = 24, alarm_able = 1;
	led = 0;
	systick_1000ms_status = 0; systick_1000ms_couter=1000;
	while(1)
	{
		for(i = 0; i < 8; i++)
			writeseg(i, student_num, 1); //显示学号
		if (systick_1000ms_status)
		{
			I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_OUTPUT_PORT2,0); //防拖影
			systick_1000ms_status = 0;
			break; //1s时间到，退出
		}
	}
	sec = 0;
}

void S800_ADC_Init(void)
{
	//
    // The ADC0 peripheral must be enabled for use.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

    //
    // Enable sample sequence 3 with a processor signal trigger.  Sequence 3
    // will do a single sample when the processor sends a singal to start the
    // conversion.  Each ADC module has 4 programmable sequences, sequence 0
    // to sequence 3.  This example is arbitrarily using sequence 3.
    //
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);

    //
    // Configure step 0 on sequence 3.  Sample the temperature sensor
    // (ADC_CTL_TS) and configure the interrupt flag (ADC_CTL_IE) to be set
    // when the sample is done.  Tell the ADC logic that this is the last
    // conversion on sequence 3 (ADC_CTL_END).  Sequence 3 has only one
    // programmable step.  Sequence 1 and 2 have 4 steps, and sequence 0 has
    // 8 programmable steps.  Since we are only doing a single conversion using
    // sequence 3 we will only configure step 0.  For more information on the
    // ADC sequences and steps, reference the datasheet.
    //
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_TS | ADC_CTL_IE |
                             ADC_CTL_END);

    //
    // Since sample sequence 3 is now configured, it must be enabled.
    //
    ADCSequenceEnable(ADC0_BASE, 3);

    //
    // Clear the interrupt status flag.  This is done to make sure the
    // interrupt flag is cleared before we sample.
    //
    ADCIntClear(ADC0_BASE, 3);
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
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);						
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOK));		
	
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);
	GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_5);
	GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE,GPIO_PIN_0 | GPIO_PIN_1);//Set the PJ0,PJ1 as input pin
	GPIOPadConfigSet(GPIO_PORTJ_BASE,GPIO_PIN_0 | GPIO_PIN_1,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPU);
	
	GPIOIntTypeSet(GPIO_PORTJ_BASE, GPIO_PIN_1, GPIO_FALLING_EDGE);
	GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_1);
	IntEnable(INT_GPIOJ);
}

void GPIOJ_Handler(void)
{
	unsigned long intStatus;
	
	intStatus = GPIOIntStatus(GPIO_PORTJ_BASE, true);
	
	GPIOIntClear(GPIO_PORTJ_BASE, intStatus);

	if (intStatus & GPIO_PIN_1)
	{
		if (hour < 8 || hour >= 21) night = !night;
	}
	SysCtlDelay( ui32SysClock / 100); 
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
	
	while(I2CMasterBusy(I2C0_BASE)){}; 
	I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false); 
	I2CMasterDataPut(I2C0_BASE, RegAddr);	
	I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START); 
	while(I2CMasterBusy(I2C0_BASE)){};
	rop = (uint8_t)I2CMasterErr(I2C0_BASE);

	I2CMasterDataPut(I2C0_BASE, WriteData);
	I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
	while(I2CMasterBusy(I2C0_BASE)){};
	rop = (uint8_t)I2CMasterErr(I2C0_BASE);

	return rop;
}

uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr)
{
	uint8_t value;
	
	while(I2CMasterBusy(I2C0_BASE)){};
	I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
	I2CMasterDataPut(I2C0_BASE, RegAddr); 
	I2CMasterControl(I2C0_BASE,I2C_MASTER_CMD_SINGLE_SEND);
	while(I2CMasterBusBusy(I2C0_BASE));
	if (I2CMasterErr(I2C0_BASE) != I2C_MASTER_ERR_NONE)
		return 0;
	Delay(100);

	//receive data
	I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, true);
	I2CMasterControl(I2C0_BASE,I2C_MASTER_CMD_SINGLE_RECEIVE);
	while(I2CMasterBusBusy(I2C0_BASE));
	if (I2CMasterErr(I2C0_BASE) != I2C_MASTER_ERR_NONE)
		return 0;
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
	SysTickPeriodSet(ui32SysClock/SYSTICK_FREQUENCY);
	SysTickEnable();
	SysTickIntEnable();
}

/*
	Corresponding to the startup_TM4C129.s vector table systick interrupt program name
*/
void SysTick_Handler(void)
{
	if (systick_1000ms_couter == 0)
	{
		systick_1000ms_couter = 1000;
		systick_1000ms_status = 1;
		sec++;
	}
	else
		systick_1000ms_couter--;
	if (systick_5s_counter == 0)
	{
		systick_5s_counter = 5000;
		systick_5s_status = 1;
	}
	else
		systick_5s_counter--;
	if (systick_500ms_counter == 0)
	{
		systick_500ms_counter = 500;
		systick_500ms_status = 1;
	}
	else
		systick_500ms_counter--;
	if (systick_250ms_counter == 0)
	{
		systick_250ms_counter = 250;
		systick_250ms_status = 1;
	}
	else
		systick_250ms_counter--;
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

	if (uart0_int_status & (UART_INT_RX | UART_INT_RT))
	{
		uart_receive_status = 1;
	}
}

