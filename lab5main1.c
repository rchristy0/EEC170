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
#include "driverlib/uart.h"
#include "drivers/rit128x96x4.h"
#include <stdio.h>

//
unsigned long ADCVal[2];
unsigned long samples[2][128];
unsigned long count= 0;

#define SEQ 2

void UARTSend(unsigned long channel){
	char string[4];
	unsigned long i, j, num;
	
	for(i= 0; i < 128; ++i){
		num= samples[channel][(count+ i)%128];
		
		for( j= 0; j < 4; ++j){
			string[3- j]= num%10+ '0';
			num/= 10;
		}
		UARTCharPut(UART0_BASE, string[0]);
		UARTCharPut(UART0_BASE, string[1]);
		UARTCharPut(UART0_BASE, string[2]);
		UARTCharPut(UART0_BASE, string[3]);
		
		UARTCharPut(UART0_BASE, ' ');
	}
	UARTCharPut(UART0_BASE, '\n');
	UARTCharPut(UART0_BASE, '\n');
}

void init(){
	// Set clock to 16.67 MHz
	SysCtlClockSet(SYSCTL_SYSDIV_16 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	
	// Status LED
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE,GPIO_PIN_0);
	GPIOPadConfigSet(GPIO_PORTF_BASE,GPIO_PIN_0,GPIO_STRENGTH_4MA,GPIO_PIN_TYPE_STD);
	
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
	
	// ADC
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC);
	ADCSequenceConfigure(ADC_BASE, SEQ, ADC_TRIGGER_TIMER, 0);
	ADCSequenceStepConfigure(ADC_BASE, SEQ, 0, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC_BASE, SEQ, 1, ADC_CTL_CH1 | ADC_CTL_IE | ADC_CTL_END);
	ADCSequenceEnable(ADC_BASE, SEQ);
	ADCIntClear(ADC_BASE, SEQ);
	ADCIntEnable(ADC_BASE, SEQ);
	IntEnable(INT_ADC2);
	
	// UART
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	GPIOPinTypeUART(GPIO_PORTA_BASE, U0TX_PIN | U0RX_PIN);
	UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));
}

int main(void){
	init();
	
	// Turn off when select is pressed
	while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));
	
	TimerDisable(TIMER0_BASE, TIMER_A);
	
	UARTSend(0);
	SysCtlDelay(SysCtlClockGet()/ 12);
	while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));
	UARTSend(1);
	
	return 0;
}

void ADCHandler(void){
	ADCIntClear(ADC_BASE, SEQ);
	ADCSequenceDataGet(ADC_BASE, SEQ, ADCVal);
	
	if(ADCVal[1] > ADCVal[0])
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0xFF);
	else
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0);
	
	samples[0][count]= ADCVal[0];
	samples[1][count]= ADCVal[1];
	if(count== 127)
		count= 0;
	else
		++count;
}
