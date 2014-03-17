#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/gpio.h"
#include "driverlib/debug.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/systick.h"
#include "drivers/rit128x96x4.h"

// Need to keep track of these values in between command strokes
volatile unsigned long prevCommand= 0x00000000, xCoord= 64, yCoord= 48, drawLevel= 15, drawSize= 0;
// Same here, but for the sampler
volatile unsigned long timeoutCount= 0, bitCount= 0, currCommand= 0, prevButton= 0;

// Size of our sample widths before timing out
#define TIME_WIDTH 0x1D4C
// Command bit-strings of the IR remote
enum{
	OK= 0x2200, ENTER= 0x2200,
	UP= 0x0200, DOWN= 0x8200, LEFT= 0xE000, RIGHT= 0x6000,
	ZERO= 0x0800, ONE= 0x8800, TWO= 0x4800, THREE= 0xC800, FOUR= 0x2800, FIVE= 0xA800, SIX= 0x6800, SEVEN= 0xE800, EIGHT= 0x1800, NINE=0x9800
};

// Draws numbers, unecessary for the assignment but convenient for debugging (and still useful)
void RIT128x96x4NumberDraw(unsigned long num, unsigned long ulX, unsigned long ulY, unsigned long level){
	char string[sizeof(unsigned long)*8 + 1];
	unsigned long msb= 0;
	
	for( msb= 1 ; msb < sizeof(unsigned long)*8; ++msb){
		string[sizeof(unsigned long)*8- msb]= num%10 + '0';
		//num-= num%10;
		num/= 10;
		
		if(num== 0)
			break;
	}
	string[sizeof(unsigned long)*8]= '\0';
	
	RIT128x96x4StringDraw(&string[sizeof(unsigned long)*8- msb], ulX, ulY, level);
}

void NumberDraw(unsigned long num){
	RIT128x96x4NumberDraw(num, xCoord, yCoord, drawLevel);
}
void StringDraw(const char *str){
	RIT128x96x4StringDraw(str, xCoord, yCoord, drawLevel);
}

// Reset the screen in between different commands
void reset(){
	xCoord= 64; yCoord= 48;
	RIT128x96x4Clear();
}

// Takes in the 32-bit bit-string of the command and executes the appropriate action
void execute(unsigned long command){
	if(prevCommand!= command){
		switch(command & 0x0000FF00){
			case OK: 
				reset(); xCoord-= 6; break;
			
			case ZERO: case ONE: case TWO: case THREE: case FOUR:
			case FIVE: case SIX: case SEVEN: case EIGHT: case NINE:
				reset(); break;
			
			default: break;
		}
	}
	
	switch(command & 0x0000FF00){
		case OK:		StringDraw("OK"); drawSize= 2; break;
		
		case UP: 		StringDraw("  "); if(yCoord!= 0) --yCoord; execute(prevCommand); prevButton= command; return;
		case DOWN:	StringDraw("  "); if(yCoord!= 96- 8) ++yCoord; execute(prevCommand); prevButton= command; return;
		case LEFT:	StringDraw("  "); if(xCoord!= 0) --xCoord; execute(prevCommand); prevButton= command; return;
		case RIGHT:	StringDraw("  "); if(xCoord!= 128- drawSize* 6+ 1) ++xCoord; execute(prevCommand); prevButton= command; return;
		
		case ZERO:	NumberDraw(0); drawSize= 1; break;
		case ONE:		NumberDraw(1); drawSize= 1; break;
		case TWO:		NumberDraw(2); drawSize= 1; break;
		case THREE:	NumberDraw(3); drawSize= 1; break;
		case FOUR:	NumberDraw(4); drawSize= 1; break;
		case FIVE:	NumberDraw(5); drawSize= 1; break;
		case SIX:		NumberDraw(6); drawSize= 1; break;
		case SEVEN:	NumberDraw(7); drawSize= 1; break;
		case EIGHT:	NumberDraw(8); drawSize= 1; break;
		case NINE:	NumberDraw(9); drawSize= 1; break;
		default:		return;
	}
	prevCommand= command; 
}

// Initialize various pins and timers
void init(){
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

int main(void){
	unsigned long pollCount= 0;
	init();
	
	// Turn off when select is pressed
	while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1)){
		if(pollCount%50000== 0){
			// If UP key pressed
			if(GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_0)== 0){
				execute(UP);
			}
			// If DOWN key pressed
			if(GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_1)== 0){
				execute(DOWN);
			}
			// If LEFT key pressed
			if(GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_2)== 0){
				execute(LEFT);
			}
			// If RIGHT key pressed
			if(GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_3)== 0){
				execute(RIGHT);
			}
		}
		++pollCount;
	}
	
	RIT128x96x4DisplayOff();
	return 0;
}

void EdgeSampler(void){
	unsigned long intType= TimerIntStatus(TIMER0_BASE, false);
	TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT | TIMER_TIMA_TIMEOUT);
	
	if(intType== TIMER_TIMA_TIMEOUT){
		++timeoutCount;
		return;
	}else if(intType== TIMER_CAPA_EVENT){
		if(timeoutCount > 5 && timeoutCount < 9){	// 0-bit size = 7
			++bitCount;
		}else if(timeoutCount > 12 && timeoutCount < 16){	// 1-bit size = 14
			currCommand|= 0x80000000 >> bitCount;
			++bitCount;
		}else	if(timeoutCount > 70 && timeoutCount < 80){	// Repeat pulse size = 76
			execute(prevButton);
		}
		
		// Always reset timeoutCount on a negative edge
		TimerLoadSet(TIMER0_BASE, TIMER_A, TIME_WIDTH);
		timeoutCount= 0;
	}
	
	// Check if our bit-string is complete, complete and reset if it is
	if(bitCount>= 32){
		prevButton= 0;
		if(prevCommand== currCommand)
			prevCommand= 0;
		execute(currCommand);
		
		currCommand= 0;
		bitCount= 0;
	}
}

