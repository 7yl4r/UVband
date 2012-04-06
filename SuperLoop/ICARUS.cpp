

#include <msp430f2013.h>
//other considered headers:
//#include "msp430.h"
//#include "io430.h"

//=== function prototypes ====================================================
void readButton();
void readSensor();

void display(float value);

void delay(unsigned long amount);
void blink();
void fadeOff();

//=== constants =============================================================
//For 24 reset loop:
const unsigned long countsUntilReset = 840000; //assuming 1 cycle through 
//for sensor reading:
const int sensorSamples = 8315; /*number of samples to take when reading sensor
NOTE: this value has been experimentally determined to be the number of samples
which are taken by the ReadSensor() function in ~.1s 
This value was chosen because the minmum sensor output is 10Hz
Due to this behavior, read frequencies are (approximately) in decaHertz*/
float freqScaler = 0.003;  /*used to scale output frequency
NOTE: this value chosen experimentally to allow for one/two lights to be on at 
the end of a day spent under flourescent light*/
//for button reading:
const int nButtonSamples = 5;
const int deltaT = 10;  //time between samples
//for display:
const int nDisplayLoops = 50; //number of loops display stays on after activation

//=== global vars ===========================================================
unsigned long count = 0;  //number of loops completed
float total = 0;      //sensor readings integrated over time [ kHz/<#hrs_since_reset> ]
float sensorValue = 0;  //last read value from sensor [ DHz ]
int buttonSamples[nButtonSamples+1];  //button samples for debounce
bool buttonState = 0;  //button open(false) or depressed(true)
int displayLoopsLeft = nDisplayLoops; //display is on whenever displayLoopsLeft > 0

int main( void )
{ 
  WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer to prevent timeout reset
  //CONFIGURE PORTS:
  //config onboard LED on P1.0
  P1DIR = 0x3F;   // set pins 1-6 for output, 7&8 for input
  P1REN = 0x80;  //set pin 8 (button) to use pull-down resistor
  P1OUT = 0x00;  //everything in P1 OFF, pull-(up/down) on 8 set to down
  
  //cycle LEDs to show successful startup
  for(int i = 3; i > 0; i--){
   P1OUT = 0x20;  //pin 1.5 on (LED5)
   delay(10000);
   P1OUT = 0x10;  //pin 1.4 on (LED4)
   delay(10000);
   P1OUT = 0x08;  //pin 1.3 on (LED3)
   delay(10000);
   P1OUT = 0x04;  //pin 1.2 on (LED2)
   delay(10000);
   P1OUT = 0x02;  //pin 1.1 on (LED1)
   delay(10000);
   P1OUT = 0x01;  //pin 1.0 on (onboard LED)
  }
  
  //=== mainloop =============================================================
  while(1){
    //timer resets system every 24hrs
      if(count > countsUntilReset){
        WDTCTL = 0; //restart
      } //implied else
      count++;

      readButton();
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
 
     readSensor();
    
    //add sensor reading to total
      float freqScaler = 0.01;  //scales down total to kHz
      total += sensorValue*freqScaler;
    
    if(displayLoopsLeft > 0){
      //onboard LED on
      P1OUT |= 0x01;
      //update display 
      displayLoopsLeft--;
      display(total);
      //fade off display if time is up
      if(displayLoopsLeft == 0){
        fadeOff();
        displayLoopsLeft = -1;
      }
    }
  }
  
   //this unreachable line puts the MSP430 into low-power 4 (just in case)
  __bis_SR_register(LPM4_bits + GIE);
}

void readButton(){
  bool sameFlag = true; //flag for all samples have same value
  //take samples at interval for compare (debounce)
  buttonSamples[0] = P1IN & 0x80;
  for(int i = 1; i < nButtonSamples; i++){
    buttonSamples[i] = P1IN & 0x80;
    if(buttonSamples[i] != buttonSamples[i-1]){
      sameFlag = false;
    }
    delay(deltaT);
  }
  if(sameFlag && (buttonSamples[0] == 0x80)){ //if all samples on
    buttonState = true;
  }
  if(sameFlag && (buttonSamples[0] == 0x00)){//if all samples off
    buttonState = false;
  }
  //else button remains in previous state   
  return;
}

void readSensor(){     /*returns frequency reading from sensor*/
  int n = 0;   //number of signal on/off and off/on changes read
  int lastRead = 0;
  //initial read
  int thisRead = P1IN & 0x40;
  //for sample time
  for(int i = sensorSamples; i > 0; i--){
    lastRead = thisRead;  //push back previous read
    thisRead = P1IN & 0x40; //read sensor
    if(thisRead != lastRead){
      n++;
    }
  }
  sensorValue = n * freqScaler;
  return;
}

void display(float value){  /*updates display with value*/
  float bin0 = 50000;  //no LEDS on iff lower than bin0
  float bin1 = 100000;
  float bin2 = 150000;
  float bin3 = 200000;
  float bin4 = 250000; //higher than bin4 means all LEDS on 
  if(value < bin0){
    P1OUT = 0x00;
  }else if(value < bin1){
    P1OUT = 0x02;
  }else if(value < bin2){
    P1OUT = 0x06;
  }else if(value < bin3){
    P1OUT = 0x0E;
  }else if(value < bin4){
    P1OUT = 0x1E;
  }else{
    P1OUT = 0x3E;
  }
  return;
}

/* very simple, power-wasting delay
 * units of 'amount' is CPU cycles
 */
void delay(unsigned long amount){ 
  for(unsigned long i = amount; i > 0; i--);
}

void blink(){
  P1OUT = 0xFF;
  delay (2000);
  P1OUT = 0x00;
  delay (2000);
}

void fadeOff(){   //fade using crude PWM
  int P1state = P1OUT;
  int nOn = 500;
  int nOff = 0;
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