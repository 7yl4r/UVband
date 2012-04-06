

#include <msp430f2013.h>
//other considered headers:
//#include "msp430.h"
//#include "io430.h"

//=== function prototypes ====================================================
void readButton();
void readSensor();

void displayTot(float value);
void displayValue(float value);

void delay(unsigned long amount);
void blink();
void fadeOff();

//=== constants =============================================================
//For 24 reset loop:
const unsigned long countsUntilReset = 10000000; //TODO: scale this to 24 hrs
//mode switching:
const int nModes = 2;   //number of display modes (counting from 0)
//for sensor reading:
const int sensorSamples = 8315; /*number of samples to take when reading sensor
NOTE: this value has been experimentally determined to be the number of samples
which are taken by the ReadSensor() function in ~.1s 
This value was chosen because the minmum sensor output is 10Hz
Due to this behavior, read frequencies are (approximately) in decaHertz*/
//for button reading:
const int nButtonSamples = 5;
const int deltaT = 10;  //time between samples
//for display:
const int nDisplayLoops = 75; //number of loops display stays on after activation

//=== global vars ===========================================================
unsigned long count = 0;  //number of loops completed
float total = 0;      //sensor readings integrated over time [ kHz/<#hrs_since_reset> ]
float sensorValue = 0;  //last read value from sensor [ DHz ]
int mode = 0;  /*used to select display mode; descriptions of modes below
  0 = always-on integrated display of total exposure amount
  1 = always-on instantaneous sensor reading
  2 = integrated display when button pushed
(real-time or integrated)
*/
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
          switch ( mode ) {
            case 0:
              break;
            case 1:
              break;
            case 2:
              displayLoopsLeft = nDisplayLoops;
              break;
            default: 
              for(int i = 10; i < 0; i--){blink();}  //blink for error
              break;
          }
          mode++;
          if(mode > nModes){
            mode = 0;
          }
      }
 
     readSensor();
    
    //add sensor reading to total
      float freqScaler = 0.01;  //scales down total to kHz
      total += sensorValue*freqScaler;
    
    if(displayLoopsLeft > 0){
      //update display based on mode selected
      switch ( mode ) {
        case 0:
          displayTot(total);
          break;
        case 1:
          displayValue(sensorValue);
          break;
        case 2:
          displayLoopsLeft--;
          displayTot(total);
          break;
        default:
          for(int i = 10; i < 0; i--){blink();}  //blink for error
          break;
      }
      //fade off display if time is up
      if(displayLoopsLeft == 0){
        fadeOff();
        displayLoopsLeft = -1;
      }
    }
    count++;
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
  float freqScaler = 1;  //can be used to scale output frequency
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

void displayTot(float value){  /*updates display with value*/
  //TODO: add display update code here
  float bin0 = 500;  //no LEDS on iff lower than bin0
  float bin1 = 1000;
  float bin2 = 1500;
  float bin3 = 2000;
  float bin4 = 2500; //higher than bin4 means all LEDS on 
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

void displayValue(float value){  /*updates display with value*/
  //TODO: adjust this scale 
  //this output assumes a read frequency of .1-100,000 DHz
  float bin0 = 10;  //no LEDS on iff lower than bin0
  float bin1 = 100;
  float bin2 = 1000;
  float bin3 = 10000;
  float bin4 = 100000;  //higher than bin4 means all LEDS on
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
    P1OUT |= 0x3E;
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
  delay (20000);
  P1OUT = 0x00;
  delay (20000);
}

void fadeOff(){   //fade using crude PWM
  int P1state = P1OUT;
  int nOn = 100;
  int nOff = 0;
  for(int i = nOn + nOff; i > 0; i--){
    nOn --;
    nOff ++;
    for(int n = nOn; n > 0; n--){
      P1OUT = P1state;
    }
    for(int n = nOff; n > 0; n--){
      P1OUT = 0x00;
    }
  }
  P1OUT = 0x00;
  blink();
}