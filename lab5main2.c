#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/gpio.h"
#include "driverlib/debug.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/adc.h"
#include "driverlib/ssi.h"
#include "drivers/rit128x96x4.h"
#include <stdio.h>

#define SEQ 2

#define CTL0 0x0900
#define CTL1 0x0A00

void init(){
	// Set clock to 16.67 MHz
	SysCtlClockSet(SYSCTL_SYSDIV_16 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	
	// Select
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	
	// Timer
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
	TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet()/ 4000);
	TimerControlTrigger(TIMER0_BASE, TIMER_A, true);
	TimerEnable(TIMER0_BASE, TIMER_A);
	
	// SSI
	SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_7);
	GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPD);
	GPIOPinTypeSSI(GPIO_PORTA_BASE, SSI0TX_PIN | SSI0CLK_PIN);
	SSIConfigSetExpClk(SSI_BASE, SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 100000, 16);
	SSIEnable(SSI_BASE);
}

int main(void){
	unsigned short count= 0;
	tBoolean up= true;
	init();
	
	// Turn off when select is pressed
	while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1)){
		
		GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0);
		SSIDataPut(SSI_BASE, CTL0 | count );
		GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0xFF);
		
		GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0);
		SSIDataPut(SSI_BASE, CTL1 | (0xFF- count) );
		GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0xFF);
		
		if(count== 0xFF)
			up= false;
		else if(count== 0)
			up= true;
		
		if(up)
			++count;
		else
			--count;
	}
	
	return 0;
}
