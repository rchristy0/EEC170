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
#include "driverlib/i2c.h"
#include "drivers/rit128x96x4.h"
#include <stdio.h>

void RIT128x96x4NumberDraw(signed long num, unsigned long ulX, unsigned long ulY, unsigned long level){
	char buffer[50];

	sprintf(buffer, "%ld", num);
	RIT128x96x4StringDraw(buffer, ulX, ulY, level);
}

/* Definitions for the pulse widths
 * Pulse 2 is Left/Right, Pulse 3 is Up/Down
 */
#define DEFAULT_PERIOD 62500

#define MIN_WIDTH_2 3640
#define DEFAULT_WIDTH_2 5280
#define MAX_WIDTH_2 6920

#define MIN_WIDTH_3 4265
#define DEFAULT_WIDTH_3 5555
#define MAX_WIDTH_3 6845

// Definitions for the Read/Write
#define READ true
#define WRITE false

// Definitions for X/Y/Z register addresses
#define SLAVE_ADDR 0x1D
#define XOUTL 0x00
#define XOUTH 0x01
#define YOUTL 0x02
#define YOUTH 0x03
#define ZOUTL 0x04
#define ZOUTH 0x05

#define XOUT8 0x06
#define YOUT8 0x07
#define ZOUT8 0x08

#define XOFFL 0x10
#define XOFFH 0x11
#define YOFFL 0x12
#define YOFFH 0x13
#define ZOFFL 0x14
#define ZOFFH 0x15

#define STATUS 0x09

// Level of OLED draws
#define LEVEL 10

// Pulse Widths
unsigned long pwmWidth2= DEFAULT_WIDTH_2, pwmWidth3= DEFAULT_WIDTH_3;
// Accelerometer Coordinates
signed char xC= 0, yC= 0, zC= 0;
signed char xOff= 0, yOff= 0, zOff= 0;

void drawOLED(){
	RIT128x96x4StringDraw("Pulse Width 2: ", 5, 40, LEVEL);
	RIT128x96x4StringDraw("Pulse Width 3: ", 5, 49, LEVEL);
	
	RIT128x96x4StringDraw("   ", 95, 40, LEVEL);
	RIT128x96x4NumberDraw(100- 100*(pwmWidth2- MIN_WIDTH_2)/(MAX_WIDTH_2- MIN_WIDTH_2), 95, 40, LEVEL);
	RIT128x96x4StringDraw("   ", 95, 49, LEVEL);
	RIT128x96x4NumberDraw(100- 100*(pwmWidth3- MIN_WIDTH_3)/(MAX_WIDTH_3- MIN_WIDTH_3), 95, 49, LEVEL);
	RIT128x96x4StringDraw("%", 112, 40, LEVEL);
	RIT128x96x4StringDraw("%", 112, 49, LEVEL);
	
	RIT128x96x4StringDraw("X-Coord: ", 5, 64, LEVEL);
	RIT128x96x4StringDraw("Y-Coord: ", 5, 73, LEVEL);
	RIT128x96x4StringDraw("Z-Coord: ", 5, 82, LEVEL);
	
	RIT128x96x4StringDraw("            ", 59, 64, LEVEL);
	RIT128x96x4NumberDraw(xC, 59, 64, LEVEL);
	RIT128x96x4StringDraw("            ", 59, 73, LEVEL);
	RIT128x96x4NumberDraw(yC, 59, 73, LEVEL);
	RIT128x96x4StringDraw("            ", 59, 82, LEVEL);
	RIT128x96x4NumberDraw(zC- 64, 59, 82, LEVEL);
}

void pollNavigation(){
	if(GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_0)== 0){
		if(pwmWidth3 > MIN_WIDTH_3)
			pwmWidth3-= 10;
		PWMPulseWidthSet(PWM_BASE, PWM_OUT_3, pwmWidth3);
	}
	if(GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_1)== 0){
		if(pwmWidth3 < MAX_WIDTH_3)
			pwmWidth3+= 10;
		PWMPulseWidthSet(PWM_BASE, PWM_OUT_3, pwmWidth3);
	}
	if(GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_2)== 0){
		if(pwmWidth2 < MAX_WIDTH_2)
			pwmWidth2+= 10;
		PWMPulseWidthSet(PWM_BASE, PWM_OUT_2, pwmWidth2);
	}
	if(GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_3)== 0){
		if(pwmWidth2 > MIN_WIDTH_2)
			pwmWidth2-= 10;
		PWMPulseWidthSet(PWM_BASE, PWM_OUT_2, pwmWidth2);
	}
}

void move(){
	signed char y= yC+ 64, x= xC;
	if(y > 127)
		y= 127;
	if(y < 0)
		y= 0;
	pwmWidth2= MAX_WIDTH_2- y*25- y%2;
	PWMPulseWidthSet(PWM_BASE, PWM_OUT_2, pwmWidth2);
	
	if(x > 64)
		x= 64;
	if(x < 0)
		x= 0;
	pwmWidth3= MIN_WIDTH_3+ x*40;
	PWMPulseWidthSet(PWM_BASE, PWM_OUT_3, pwmWidth3);
}

unsigned long getSlaveValue(unsigned long ulBase, unsigned char ucRegisterAddr){
	I2CMasterSlaveAddrSet(ulBase, SLAVE_ADDR, WRITE);
	I2CMasterDataPut(ulBase, ucRegisterAddr);
	I2CMasterControl(ulBase, I2C_MASTER_CMD_BURST_SEND_START);
	while(I2CMasterBusy(ulBase));

	I2CMasterSlaveAddrSet(ulBase, SLAVE_ADDR, READ);
	I2CMasterControl(ulBase, I2C_MASTER_CMD_SINGLE_RECEIVE);
	while(I2CMasterBusy(ulBase));
	
	return I2CMasterDataGet(ulBase);
}

void setSlaveValue(unsigned long ulBase, unsigned char ucRegisterAddr, signed char val){
	I2CMasterSlaveAddrSet(ulBase, SLAVE_ADDR, WRITE);
	I2CMasterDataPut(ulBase, ucRegisterAddr);
	I2CMasterControl(ulBase, I2C_MASTER_CMD_BURST_SEND_START);
	while(I2CMasterBusy(ulBase));
	
	I2CMasterDataPut(ulBase, val);
	I2CMasterControl(ulBase, I2C_MASTER_CMD_BURST_SEND_FINISH);
	while(I2CMasterBusy(ulBase));
}

void pollAccelerometer(){
	xC= getSlaveValue(I2C0_MASTER_BASE, XOUT8);
	yC= getSlaveValue(I2C0_MASTER_BASE, YOUT8);
	zC= getSlaveValue(I2C0_MASTER_BASE, ZOUT8);
	
	xC+= xOff;
	yC+= yOff;
	zC+= zOff;
}

void calibrateAccelerometer(){
	char i= 0;
	setSlaveValue(I2C0_MASTER_BASE, 0x16, 0x5);

	setSlaveValue(I2C0_MASTER_BASE, XOFFL, 0);
	setSlaveValue(I2C0_MASTER_BASE, XOFFH, 0);
	setSlaveValue(I2C0_MASTER_BASE, YOFFL, 0);
	setSlaveValue(I2C0_MASTER_BASE, YOFFH, 0);
	setSlaveValue(I2C0_MASTER_BASE, ZOFFL, 0);
	setSlaveValue(I2C0_MASTER_BASE, ZOFFH, 0);
	
	for(i= 0; i < 10; ++i){
		while(!(getSlaveValue(I2C0_MASTER_BASE, STATUS) & 0x1));
		pollAccelerometer();
		
		xOff-= xC;
		yOff-= yC;
		zOff-= zC;
	}
}

void init(){
	// Init display
	RIT128x96x4Init(1000000);
	
	// Set clock to 50 MHz
	SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	SysCtlPWMClockSet(SYSCTL_PWMDIV_16);
	
	// Select
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	
	// Navigation Switches
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	
	// Enable PWM Pins
  SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM); 
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	GPIOPinTypePWM(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);
	
	// Configure PWM signals
	PWMGenConfigure(PWM_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
	PWMGenPeriodSet(PWM_BASE, PWM_GEN_1, DEFAULT_PERIOD);
	PWMPulseWidthSet(PWM_BASE, PWM_OUT_2, pwmWidth2);
	PWMPulseWidthSet(PWM_BASE, PWM_OUT_3, pwmWidth3);
	PWMOutputState(PWM_BASE, PWM_OUT_2_BIT | PWM_OUT_3_BIT, true);
	PWMGenEnable(PWM_BASE, PWM_GEN_1);
	
	// I2C Initialization
	SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
	I2CMasterInitExpClk(I2C0_MASTER_BASE, SysCtlClockGet(), false);
	
	// I2C Data Pins
	// B2 is SCL, B3 is SDA
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_2 | GPIO_PIN_3);
	GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_OD);
	GPIOPinConfigure(I2C0SCL_PIN | I2C0SDA_PIN);
  //GPIODirModeSet(GPIO_PORTB_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_DIR_MODE_HW);
	
	// Configure XYZ Offsets
	calibrateAccelerometer();
}

int main(void){
	unsigned long pollCount= 0;
	init();
	
	// Turn off when select is pressed
	while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1)){
		if(pollCount%50000== 0){
			pollNavigation();
			if(getSlaveValue(I2C0_MASTER_BASE, STATUS) & 0x1){
				pollAccelerometer();
				move();
			}
			drawOLED();
		}
		++pollCount;
	}
	
	RIT128x96x4DisplayOff();
	return 0;
}
