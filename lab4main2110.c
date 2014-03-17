#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "inc/hw_i2c.h"
#include "driverlib/gpio.h"
#include "driverlib/debug.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/pwm.h"
#include "driverlib/uart.h"




/* Definitions for the pulse widths
 * Pulse 0 is Left/Right, Pulse 1 is Up/Down
 */
#define DEFAULT_PERIOD 62500

#define MIN_WIDTH_0 3640
#define DEFAULT_WIDTH_0 5280
#define MAX_WIDTH_0 6920

#define MIN_WIDTH_1 4280
#define DEFAULT_WIDTH_1 5570
#define MAX_WIDTH_1 6860

// Pulse Widths
unsigned long pwmWidth0= DEFAULT_WIDTH_0, pwmWidth1= DEFAULT_WIDTH_1;
unsigned long count= 0;

unsigned char msg[8];

void init(){
	// Set clock to 25 Mhz
	SysCtlClockSet(SYSCTL_SYSDIV_8 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	SysCtlPWMClockSet(SYSCTL_PWMDIV_8);
	
	// Enable PWM Pins
  SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM); 
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);
	
	// Configure PWM signals
	PWMGenConfigure(PWM_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
	PWMGenPeriodSet(PWM_BASE, PWM_GEN_0, DEFAULT_PERIOD);
	PWMPulseWidthSet(PWM_BASE, PWM_OUT_0, pwmWidth0);
	PWMPulseWidthSet(PWM_BASE, PWM_OUT_1, pwmWidth1);
	PWMOutputState(PWM_BASE, PWM_OUT_0_BIT | PWM_OUT_1_BIT, true);
	PWMGenEnable(PWM_BASE, PWM_GEN_0);
	
	// Configure UART
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  GPIOPinTypeUART(GPIO_PORTA_BASE, U0TX_PIN | U0RX_PIN);
  UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600,
                      (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                       UART_CONFIG_PAR_NONE));
  IntEnable(INT_UART0);
  UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
}

int main(){
	init();
	
	while(1);
	
	return 0;
}

/* In Startup.S, added:
 *
 * EXTERN UARTHandler
 *
 * Under the EXPORT __Vectors, changed this line:
 *
 * DCD     UARTHandler					; UART0 Rx and Tx
 *
 */
void UARTHandler(void){
	unsigned long ulStatus;
	ulStatus = UARTIntStatus(UART0_BASE, true);
	UARTIntClear(UART0_BASE, ulStatus);
	
	// Each 4 bytes are expected to be an unsigned long,
	// so we treat the memory space as such
	while(UARTCharsAvail(UART0_BASE)){
		msg[count]= UARTCharGet(UART0_BASE);
		++count;
		
		//Every 8 characters, read them into the pulse widths
		if(count== 8){
			pwmWidth0= msg[0] << 24;
			pwmWidth0|= msg[1] << 16;
			pwmWidth0|= msg[2] << 8;
			pwmWidth0|= msg[3];
			
			pwmWidth1= msg[4] << 24;
			pwmWidth1|= msg[5] << 16;
			pwmWidth1|= msg[6] << 8;
			pwmWidth1|= msg[7];

			PWMPulseWidthSet(PWM_BASE, PWM_OUT_0, pwmWidth0);
			PWMPulseWidthSet(PWM_BASE, PWM_OUT_1, pwmWidth1);
			
			UARTCharPut(UART0_BASE, msg[0]);
			while(UARTBusy(UART0_BASE));
			UARTCharPut(UART0_BASE, msg[1]);
			while(UARTBusy(UART0_BASE));
			UARTCharPut(UART0_BASE, msg[2]);
			while(UARTBusy(UART0_BASE));
			UARTCharPut(UART0_BASE, msg[3]);
			while(UARTBusy(UART0_BASE));
			UARTCharPut(UART0_BASE, msg[4]);
			while(UARTBusy(UART0_BASE));
			UARTCharPut(UART0_BASE, msg[5]);
			while(UARTBusy(UART0_BASE));
			UARTCharPut(UART0_BASE, msg[6]);
			while(UARTBusy(UART0_BASE));
			UARTCharPut(UART0_BASE, msg[7]);
			while(UARTBusy(UART0_BASE));
	
			count= 0;
		}
	}
}
