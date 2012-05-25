
#include <msp430f2013.h>
//other considered headers:
//#include "msp430.h"
//#include "io430.h"

//hardware definitions
#define BUTTON_BIT 0x40
#define SENSOR_ENABLE_BIT 0x01
#define SENSOR_BIT 0x80
#define LED0 0x02
#define LED1 0x04
#define LED2 0x08
#define LED3 0x10
#define LED4 0x20

int P1OUTMASK = SENSOR_ENABLE_BIT + LED0 + LED1 + LED2 + LED3 + LED4; 

//=== function prototypes ====================================================
void readButton();
void readSensor();

void display(float value);

void delay(unsigned long amount);
void fade(int nOn, int nOff);

//=== constants =============================================================
//For 24 reset loop:
const unsigned int countsUntilReset = 8400; //assuming 1 cycle through 

//for sensor reading:
const int sensorSamples = 8315; /*number of samples to take when reading sensor
NOTE: this value has been experimentally determined to be the number of samples
which are taken by the ReadSensor() function in ~.1s 
This value was chosen because the minmum sensor output is 10Hz
Due to this behavior, read frequencies are (approximately) in decaHertz*/

float freqScaler = 1.0;/*0.003;  /*used to scale output frequency
NOTE: this value chosen experimentally to allow for one/two lights to be on at 
the end of a day spent under flourescent light*/

//for button reading:
const int nButtonSamples = 5;
const int deltaT = 1;  //time between samples

//for display:
const int nDisplayLoops = 10; //number of loops display stays on after activation

//=== global vars ===========================================================
unsigned long count = 0;  //number of loops completed
float total = 0;      //sensor readings integrated over time [ kHz/<#hrs_since_reset> ]
float sensorValue = 0;  //last read value from sensor [ DHz ]
int buttonSamples[nButtonSamples+1];  //button samples for debounce
bool buttonState = 0;  //button open(false) or depressed(true)
int displayLoopsLeft = nDisplayLoops; //display is on whenever displayLoopsLeft > 0

int main( void )
{ 
  // === STARTUP =======================
  WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer to prevent timeout reset
  //===CONFIGURE PORTS:
  //config onboard LED on P1.0
  P1DIR = P1OUTMASK;  // set pins 1-6 for output, 7&8 for input
  P1REN = BUTTON_BIT;  //set p1.6 (button) to use pull-down resistor
  P1OUT = 0x00;  //everything in P1 OFF, pull-(up/down)s set to down
  //cycle LEDs to show successful startup
  for(int i = 3; i > 0; i--){
   P1OUT = LED4 & P1OUTMASK;  //pin 1.5 on (LED5)
   delay(10000);
   P1OUT = LED3 & P1OUTMASK;  //pin 1.4 on (LED4)
   delay(10000);
   P1OUT = LED2 & P1OUTMASK;  //pin 1.3 on (LED3)
   delay(10000);
   P1OUT = LED1 & P1OUTMASK;  //pin 1.2 on (LED2)
   delay(10000);
   P1OUT = LED0 & P1OUTMASK;  //pin 1.1 on (LED1)
   delay(10000);
  }
  P1OUT = 0x00 & P1OUTMASK;   //clear display
  //===SETUP THE TIMER
  //TODO: what is this? TimerA Control = TimerA???_2 + MC??_2 + TimerAInterruptEnable
  TACTL = TASSEL_2 + MC_2 + TAIE;   //SMCLK, contmode, interrupt
  //===SETUP THE BUTTON
  //TODO: this should be set to the pin on which the button is located (P1.7???)
  P1IE |= BUTTON_BIT; //enable interrupt on P1 button
  P1IES&=~BUTTON_BIT; //set interrupt mode on P1 to detect LOW-HIGH edge
  P1IFG&=~BUTTON_BIT; //clear interrupt flag on P1
  //put the MSP430 into low-power mode
  while(1){
  __bis_SR_register(LPM0_bits + GIE); //enter low-power 0 w/ interrupt
  }
}

void readButton(){
  bool sameFlag = true; //flag for all samples have same value
  //take samples at interval for compare (debounce)
  buttonSamples[0] = P1IN & BUTTON_BIT;
  for(int i = 1; i < nButtonSamples; i++){
    buttonSamples[i] = P1IN & BUTTON_BIT;
    if(buttonSamples[i] != buttonSamples[i-1]){
      sameFlag = false;
    }
    delay(deltaT);
  }
  if(sameFlag && (buttonSamples[0] == BUTTON_BIT)){ //if all samples on
    buttonState = true;
  }
  if(sameFlag && (buttonSamples[0] == 0x00)){//if all samples off
    buttonState = false;
  }
  //else button remains in previous state   
  return;
}
void readSensor(){     //returns frequency reading from sensor
  int n = 0;   //number of signal on/off and off/on changes read
  int lastRead = 0;
  //initial read
  int thisRead = P1IN & SENSOR_BIT;
  //for sample time
  for(int i = sensorSamples; i > 0; i--){
    lastRead = thisRead;  //push back previous read
    thisRead = P1IN & SENSOR_BIT; //read sensor
    if(thisRead != lastRead){
      n++;
    }
  }
  sensorValue = n * freqScaler;
  return;
}

void display(float value){  //updates display with value
  float bin0 = 1000;  //no LEDS on iff lower than bin0
  float bin1 = 2000;
  float bin2 = 3000;
  float bin3 = 4000;
  float bin4 = 5000; //higher than bin4 means all LEDS on
  if(value < bin0){
    P1OUT = LED0 & P1OUTMASK;
  }else if(value < bin1){
    P1OUT = (LED0 + LED1) & P1OUTMASK;
  }else if(value < bin2){
    P1OUT = (LED0 + LED1 + LED2) & P1OUTMASK;
  }else if(value < bin3){
    P1OUT = (LED0 + LED1 + LED2 + LED3) & P1OUTMASK;
  }else if(value < bin4){
    P1OUT = (LED0 + LED1 + LED2 + LED3 + LED4) & P1OUTMASK;
  }else{
    //flash if value is this high
    P1OUT ^= (LED0 + LED2 + LED4) & P1OUTMASK;
  }
  
  return;
}

// very simple, power-wasting delay
// *NOTE: units of 'amount' is CPU cycles
void delay(unsigned long amount){ 
  for(unsigned long i = amount; i > 0; i--);
}

//fade using crude PWM
//fade(int initialTimeOn, int endingTimeOn)
void fade(int nOn, int nOff){   
  int P1state = P1OUT;
  for(int i = nOn + nOff; i > 0; i--){
    nOn --;
    nOff ++;
    //turn off
    P1OUT = 0x00;
    delay(nOff);
    //turn on
    P1OUT = P1state;
    delay(nOn);
  }
  P1OUT = 0x00;
}

//Timer_A3 Interrupt Vector (TAIV) handler
#pragma vector=TIMERA1_VECTOR
__interrupt void Timer_A(void){
  
  //TODO: what is this?
  switch(TAIV){
  case 2: break;  //CCR1 not used
  case 4: break;  //CCR2 not used
  //case 10: P1OUT ^= 0x01 & P1OUTMASK;   //overflow
    break;
  }
  //add to runtime clock
  count ++;
  //timer resets system every 24hrs
  if(count > countsUntilReset){
    WDTCTL = 0; //restart
  } //implied else
  
  if (count % 10 == 0){
      // === TAKE SENSOR READING =============
    P1OUT |= SENSOR_ENABLE_BIT & P1OUTMASK;  //turn sensor on
    readSensor();
    P1OUT ^= SENSOR_ENABLE_BIT & P1OUTMASK; //turn sensor off
    //add sensor reading to total
    float freqScaler = 0.01;  //scales down total to kHz
    total += sensorValue*freqScaler;
    
    // === UPDATE DISPLAY ==================
    if(displayLoopsLeft > 0){
      //onboard LED on
      //P1OUT |= 0x01;
      //update display 
      displayLoopsLeft--;
      display(total);
      //fade off display if time is up
      if(displayLoopsLeft == 0){
        fade(200, 0);
        displayLoopsLeft = -1;
      }
    }
  }
}


//Port 1 interrupt service routine (BUTTON)A
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void){
  //TODO: handle the button press here
  
  //test blink when pressed
  P1OUT  = (LED0 + LED1 + LED2 + LED3 + LED4) & P1OUTMASK; // P1 toggle
  
  readButton();

 /*
  P1IES ^= BUTTON_BIT; // toggle the interrupt edge,
  // the interrupt vector will be called
  // when P1.3 goes from HitoLow as well as
  // LowtoHigh
  */
  
  if(buttonState == true){  //if button pressed, count amount of time held
      //count time button is held
      long onCount = 0;
      do{ 
        readButton();
        onCount++;
        if(onCount > 10000){ //if held a long time
          WDTCTL = 0; //restart
        } //implied else
      } while(buttonState == true);
      //if button pressed & released
      displayLoopsLeft = nDisplayLoops;
    }
  
  P1IFG &= ~BUTTON_BIT; // P1.3 IFG cleared
}

