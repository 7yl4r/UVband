

#include <msp430f2013.h>
//other considered headers:
//#include "msp430.h"
//#include "io430.h"

//=== function prototypes ====================================================
void configClocks();
void configPorts();

void readButton();
void readSensor();

void displayTot(float value);
void displayValue(float value);

void delay(unsigned long amount);
void blink();

//=== constants =============================================================
const unsigned long countsUntilReset = 10000000; //TODO: scale this to 24 hrs
const int nModes = 1;   //number of display modes (counting from 0)

//=== global vars ===========================================================
unsigned long count = 0;  //number of loops completed
float total = 0;      //sensor readings integrated over time
float sensorValue = 0;  
int mode = 0;  //used to select display mode (real-time or integrated)
bool buttonState = 0;  //button open(false) or depressed(true)

int main( void )
{ 
  //=== setup ===========
  WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer to prevent timeout reset
  configPorts();
  
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
  
  //turn on onboard (power light) also for use as button input
  P1OUT = 0x01;  //pin 1.0 on (onboard LED)
  
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
          if(onCount > 1000){ //if held a long time
            WDTCTL = 0; //restart
          } //implied else
        } while(buttonState == true);
        //if just pressed & released toggle mode
          mode++;
          if(mode > nModes){
            mode = 0;
          }
      } 
 
      //P1OUT ^= 0x01; //toggle P1.0
     readSensor();
    
    //add sensor reading to total
      total += sensorValue;     //TODO: change this line to scale down total
    
    //update display
    switch ( mode ) {
      case 0:
        displayTot(total);
        break;
      case 1:
        displayValue(sensorValue);
        break;
      default:
        blink();
    }
    count++;
  }
  
   //this unreachable line puts the MSP430 into low-power 4 (just in case)
  __bis_SR_register(LPM4_bits + GIE);
}

void configPorts(){
  //config onboard LED on P1.0
  P1DIR = 0x3F;   // set pins 1-6 for output, 7&8 for input
  P1OUT = 0x00;  //everything in P1 OFF
  return;
}

void readButton(){
  const int nButtonSamples = 3;
  int buttonSamples[nButtonSamples+1];  //button samples for debounce
  int deltaT = 10;  //time between samples
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
  long n = 0;   //number of signal on/off and off/on changes read
  int lastRead = 0;
  float freqScaler = .005;  //used to scale read frequency
  //initial read
  int thisRead = P1IN & 0x40;
  //for sample time
  for(long i = 1000; i > 0; i--){
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
  } else if(value <bin1){
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
  //turn P1.0 (onboard) back on
  P1OUT |= 0x01;
  return;
}

void displayValue(float value){  /*updates display with value*/
  //TODO: add display update code here
  float bin0 = 1;  //no LEDS on iff lower than bin0
  float bin1 = 2;
  float bin2 = 3;
  float bin3 = 4;
  float bin4 = 5;  //higher than bin4 means all LEDS on
  if(value < bin0){
    P1OUT = 0x00;
  } else if(value <bin1){
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
  //turn P1.0 (onboard) back on
  P1OUT |= 0x01;
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