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
#include "driverlib/ssi.h"
#include "drivers/rit128x96x4.h"
#include <stdio.h>

void Window16to32b_real( int *, unsigned short *, int );
void FFT128Real_32b(int *, int *);
void magnitude32_32bIn( int *, int );
unsigned short w[128/ 2];
int y[128+ 2];

//
unsigned long ADCVal[2];
unsigned long samples[2][128];
unsigned long count= 0, counter= 0;

#define SEQ 2
#define SAMPLE_FREQ 4000

#define CTL0 0x0900
#define CTL1 0x0A00

void UARTSend(unsigned long num){
	char string[4];
	unsigned long j;
	
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

unsigned long getFrequency(unsigned long channel){
	int max= 0, maxi, i;
	int *x= (int *) &(samples[channel][0]);
	
	Window16to32b_real( x, w, 128);
	FFT128Real_32b( y, x);
	magnitude32_32bIn( &(y[2]), (128/ 2)- 1);
	
	for(i= 2; i < 128+ 2; i+= 2){
		if(y[i] > max){
			max= y[i];
			maxi= i;
		}
	}
	
	return (16* SAMPLE_FREQ* maxi)/(128* 2* 24);
}

void init(){
	int i;
	// Set clock to 16.67 MHz
	SysCtlClockSet(SYSCTL_SYSDIV_16 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	
	// Select
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	
	// Timer
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
	TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet()/ SAMPLE_FREQ);
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
	
	// SSI
	SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_7);
	GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPD);
	GPIOPinTypeSSI(GPIO_PORTA_BASE, SSI0TX_PIN | SSI0CLK_PIN);
	SSIConfigSetExpClk(SSI_BASE, SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 100000, 16);
	SSIEnable(SSI_BASE);
	
	// UART
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	GPIOPinTypeUART(GPIO_PORTA_BASE, U0TX_PIN | U0RX_PIN);
	UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));
												 
	// FFT
	for( i= 0; i < 64; ++i){
		w[i]= 0xFFFF;
	}
}

int main(void){
	init();
	
	// Turn off when select is pressed
	while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));
	
	return 0;
}

void ADCHandler(void){
	++counter;
	ADCIntClear(ADC_BASE, SEQ);
	ADCSequenceDataGet(ADC_BASE, SEQ, ADCVal);
	
	samples[0][count]= ADCVal[0];
	samples[1][count]= ADCVal[1];
	if(count== 127)
		count= 0;
	else
		++count;

	if(counter%4000== 0){
		UARTSend(getFrequency(0));
		UARTSend(getFrequency(1));
		UARTCharPut(UART0_BASE, 0);
	}
	
	GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0);
	SSIDataPut(SSI_BASE, CTL0 |  (ADCVal[0] >> 2));
	while(SSIBusy(SSI_BASE));
	GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0xFF);
	
	
	GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0);
	SSIDataPut(SSI_BASE, CTL1 |  (ADCVal[1] >> 2));
	while(SSIBusy(SSI_BASE));
	GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, 0xFF);
}
