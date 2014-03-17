#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/gpio.h"
#include "driverlib/debug.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "drivers/rit128x96x4.h"
#include <stdio.h>

// Pulse width values
#define MIN_WIDTH_0 3640
#define DEFAULT_WIDTH_0 5350
#define MAX_WIDTH_0 6920

#define MIN_WIDTH_1 4280
#define DEFAULT_WIDTH_1 5570
#define MAX_WIDTH_1 6850

// Need to keep track of these values in between command strokes
volatile unsigned long prevCommand= 0x00000000, drawLevel= 15;
volatile unsigned long pwm0= DEFAULT_WIDTH_0, pwm1= DEFAULT_WIDTH_1;
// Same here, but for the sampler
volatile unsigned long timeoutCount= 0, bitCount= 0, currCommand= 0, prevButton= 0;
double currNum= 0;

unsigned char msg[8];
tBoolean DONE= false;

// Size of our sample widths before timing out
#define TIME_WIDTH 0x1D4C

// Command bit-strings of the IR remote
enum{
	OK= 0x2200, ENTER= 0x2200,
	
	UP= 0x0200, DOWN= 0x8200, LEFT= 0xE000, RIGHT= 0x6000,
	
	ZERO= 0x0800, ONE= 0x8800, TWO= 0x4800, THREE= 0xC800, FOUR= 0x2800,
	FIVE= 0xA800, SIX= 0x6800, SEVEN= 0xE800, EIGHT= 0x1800, NINE=0x9800
};

void RIT128x96x4NumberDraw(unsigned long num, unsigned long ulX, unsigned long ulY, unsigned long level){
	char string[sizeof(unsigned long)*8 + 1];
	unsigned long msb= 0;
	
	for( msb= 1 ; msb < sizeof(unsigned long)*8; ++msb){
		string[sizeof(unsigned long)*8- msb]= num%10 + '0';
		num/= 10;
		
		if(num== 0)
			break;
	}
	string[sizeof(unsigned long)*8]= '\0';
	
	RIT128x96x4StringDraw(&string[sizeof(unsigned long)*8- msb], ulX, ulY, level);
}

void send(){
	// Bounds-check each pwm signal
	if(pwm0 > MAX_WIDTH_0)
		pwm0= MAX_WIDTH_0;
	else if(pwm0 < MIN_WIDTH_0)
		pwm0= MIN_WIDTH_0;
	
	if(pwm1 > MAX_WIDTH_1)
		pwm1= MAX_WIDTH_1;
	else if(pwm1 < MIN_WIDTH_1)
		pwm1= MIN_WIDTH_1;
	
	// Write both unsigned longs to the 8-byte space and send it
	// Write both unsigned longs to the 8-byte space and send it
	msg[0]= (pwm0 & 0xFF000000) >> 24;
	msg[1]= (pwm0 & 0x00FF0000) >> 16;
	msg[2]= (pwm0 & 0x0000FF00) >> 8;
	msg[3]= pwm0 & 0x000000FF;
	msg[4]= (pwm1 & 0xFF000000) >> 24;
	msg[5]= (pwm1 & 0x00FF0000) >> 16;
	msg[6]= (pwm1 & 0x0000FF00) >> 8;
	msg[7]= pwm1 & 0x000000FF;
	
	UARTCharPut(UART1_BASE, msg[0]);
	while(UARTBusy(UART1_BASE));
	UARTCharPut(UART1_BASE, msg[1]);
	while(UARTBusy(UART1_BASE));
	UARTCharPut(UART1_BASE, msg[2]);
	while(UARTBusy(UART1_BASE));
	UARTCharPut(UART1_BASE, msg[3]);
	while(UARTBusy(UART1_BASE));
	UARTCharPut(UART1_BASE, msg[4]);
	while(UARTBusy(UART1_BASE));
	UARTCharPut(UART1_BASE, msg[5]);
	while(UARTBusy(UART1_BASE));
	UARTCharPut(UART1_BASE, msg[6]);
	while(UARTBusy(UART1_BASE));
	UARTCharPut(UART1_BASE, msg[7]);
	while(UARTBusy(UART1_BASE));
}

// Takes in the 32-bit bit-string of the command and executes the appropriate action
void execute(unsigned long command){
	// Does what the command is supposed to do
	switch(command & 0x0000FF00){
		case OK:		RIT128x96x4StringDraw("   ", 64, 67, drawLevel);
								if(DONE){
									pwm1= ((90- currNum))*(MAX_WIDTH_1- MIN_WIDTH_1)/90+ MIN_WIDTH_1;
									currNum= 0;
									send();
									DONE= false;
								}else{
									pwm0= ((359- currNum))*(MAX_WIDTH_0- MIN_WIDTH_0)/359+ MIN_WIDTH_0;
									currNum= 0;
									DONE= true;
								}
								return;
		
		case UP: 		pwm1-= 100; prevButton= command; send(); return;
		case DOWN:	pwm1+= 100; prevButton= command; send(); return;
		case LEFT:	pwm0+= 100; prevButton= command; send(); return;
		case RIGHT:	pwm0-= 100; prevButton= command; send(); return;
		
		case ZERO:	currNum*= 10; break;
		case ONE:		currNum*= 10; currNum+= 1; break;
		case TWO:		currNum*= 10; currNum+= 2; break;
		case THREE:	currNum*= 10; currNum+= 3; break;
		case FOUR:	currNum*= 10; currNum+= 4; break;
		case FIVE:	currNum*= 10; currNum+= 5; break;
		case SIX:		currNum*= 10; currNum+= 6; break;
		case SEVEN:	currNum*= 10; currNum+= 7; break;
		case EIGHT:	currNum*= 10; currNum+= 8; break;
		case NINE:	currNum*= 10; currNum+= 9; break;
		default:		return;
	}
	RIT128x96x4StringDraw("   ", 64, 67, drawLevel);
	RIT128x96x4NumberDraw(currNum, 64, 67, drawLevel);
}

void init(){
	// Init OLED
  RIT128x96x4Init(1000000);
	
	// Set the clock to 50 MHz
	SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	
	// Select
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);	
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	
	// Navigation Switches
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	
	// Configure UART
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
  GPIOPinTypeUART(GPIO_PORTD_BASE, U1TX_PIN | U1RX_PIN);
  UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 9600,
                      (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                       UART_CONFIG_PAR_NONE));
  IntEnable(INT_UART1);
  UARTIntEnable(UART1_BASE, UART_INT_RX | UART_INT_RT);
	
	// IR Receiver
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);	
	GPIOPinTypeTimer(GPIO_PORTD_BASE, GPIO_PIN_4);
	
	// Timer
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);	
	TimerConfigure(TIMER0_BASE, TIMER_CFG_A_CAP_TIME);
	TimerControlEvent(TIMER0_BASE, TIMER_A, TIMER_EVENT_NEG_EDGE);
	TimerLoadSet(TIMER0_BASE, TIMER_A, TIME_WIDTH);
	TimerIntEnable(TIMER0_BASE, TIMER_CAPA_EVENT | TIMER_TIMA_TIMEOUT);
	TimerEnable(TIMER0_BASE, TIMER_A);
	IntEnable(INT_TIMER0A);
}

int main(){
	init();
	
	// Send default pulse widths upon startup
	send();
	
	// Turn off when select is pressed
	while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1)){
		RIT128x96x4StringDraw("PWM Angle 0: ", 5, 40, drawLevel);
		RIT128x96x4StringDraw("PWM Angle 1: ", 5, 49, drawLevel);
	}
	
	RIT128x96x4DisplayOff();
		
	return 0;
}

/* In Startup.S, added:
 *
 * EXTERN UARTHandler
 *
 * Under the EXPORT __Vectors, changed this line:
 *
 * DCD     UARTHandler           ; UART1 Rx and Tx
 *
 */
void UARTHandler(void){
	unsigned long ulStatus, count= 0;
	ulStatus = UARTIntStatus(UART1_BASE, true);
	UARTIntClear(UART1_BASE, ulStatus);
	
	// Each 4 bytes are expected to be an unsigned long,
	// so we treat the memory space as such
	while(UARTCharsAvail(UART1_BASE)){
		msg[count]= UARTCharGet(UART1_BASE);
		++count;
		
		//Every 8 characters, read them into the pulse widths
		if(count== 8){
			pwm0= msg[0] << 24;
			pwm0|= msg[1] << 16;
			pwm0|= msg[2] << 8;
			pwm0|= msg[3];
			
			pwm1= msg[4] << 24;
			pwm1|= msg[5] << 16;
			pwm1|= msg[6] << 8;
			pwm1|= msg[7];
	
			RIT128x96x4StringDraw("     ", 83, 40, drawLevel);
			RIT128x96x4NumberDraw(359- 359*(pwm0- MIN_WIDTH_0)/(MAX_WIDTH_0- MIN_WIDTH_0), 95, 40, drawLevel);
			RIT128x96x4StringDraw("     ", 83, 49, drawLevel);
			RIT128x96x4NumberDraw(90- 90*(pwm1- MIN_WIDTH_1)/(MAX_WIDTH_1- MIN_WIDTH_1), 95, 49, drawLevel);
			
			count= 0;
		}
	}
	
}

/* In Startup.S, added:
 *
 * EXTERN EdgeSampler
 *
 * Under the EXPORT __Vectors, changed this line:
 *
 * DCD     EdgeSampler					; Timer 0 subtimer A
 *
 */
void EdgeSampler(void){
	unsigned long intType= TimerIntStatus(TIMER0_BASE, false);
	TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT | TIMER_TIMA_TIMEOUT);
	
	// If we have a timeout, return instantly
	if(intType== TIMER_TIMA_TIMEOUT){
		++timeoutCount;
		return;
		
	// If we catch an edge, check the period width (measured in timeouts)
	}else if(intType== TIMER_CAPA_EVENT){
		if(timeoutCount > 5 && timeoutCount < 9){	// 0-bit size
			++bitCount;
		}else if(timeoutCount > 12 && timeoutCount < 16){	// 1-bit size
			currCommand|= 0x80000000 >> bitCount;
			++bitCount;
		}else	if(timeoutCount > 70 && timeoutCount < 80){	// Repeat pulse size
			execute(prevButton);
		}
		
		// Always reset timeoutCount on a negative edge
		TimerLoadSet(TIMER0_BASE, TIMER_A, TIME_WIDTH);
		timeoutCount= 0;
	}
	
	// Check if our bit-string is complete, execute it and reset if it is
	if(bitCount>= 32){
		prevButton= 0;
		execute(currCommand);
		
		currCommand= 0;
		bitCount= 0;
	}
}
