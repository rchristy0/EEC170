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
#include "driverlib/can.h"




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

tCANMsgObject receive;
unsigned char msg[8]= "\0\0\0\0\0\0\0\0";

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
	
	// CAN Connection
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_CAN0);
	GPIOPinTypeCAN(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	CANInit(CAN0_BASE);
	CANBitRateSet(CAN0_BASE, 8000000, 250000);
	CANEnable(CAN0_BASE);
	CANIntEnable(CAN0_BASE, CAN_INT_MASTER);
	IntEnable(INT_CAN0);
	
	// CAN Objects
	receive.ulMsgID= 0x200;
	receive.ulMsgIDMask= 0xFFF;
	receive.ulMsgLen= 2*sizeof(unsigned long);
	receive.ulFlags= MSG_OBJ_USE_ID_FILTER | MSG_OBJ_RX_INT_ENABLE;
	receive.pucMsgData= msg;
	CANMessageSet(CAN0_BASE, 1, &receive, MSG_OBJ_TYPE_RX);
}

int main(){
	init();
	
	while(1);
	
	return 0;
}

/* In Startup.S, added:
 *
 * EXTERN CANHandler
 *
 * Under the EXPORT __Vectors, changed this line:
 *
 * DCD     CANHandler					; CAN0
 *
 */
void CANHandler(void){
	unsigned long ulStatus;
	
	ulStatus= CANIntStatus(CAN0_BASE, CAN_INT_STS_CAUSE);
	CANIntClear(CAN0_BASE, ulStatus);
	
	receive.pucMsgData= msg;
	CANMessageGet(CAN0_BASE, 1, &receive, true);
	
	// Each 4 bytes are expected to be an unsigned long,
	// so we treat the memory space as such
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
}
