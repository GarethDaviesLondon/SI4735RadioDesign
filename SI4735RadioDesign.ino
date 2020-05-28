#define DEBUG
//#define UPLOADPATCH
//#include "patch_init.h"

#include "GWDSI4735.h"
#include "Rotary.h"
#include <Wire.h>             //Needed by the SPI library
#include <Adafruit_GFX.h>     //Used for the OLED display, called by the SSD1306 Library
#include <Adafruit_SSD1306.h> //This is the OLED driver library
#include <EEPROM.h>

GWDSI4735 si4735;
//SI4735 si4735;




//ROTARY ENCODER parameters
#define ROTARYLEFT 2  //Pin the left turn on the encoder is connected to Arduino
#define ROTARYRIGHT 3 //Pin the right turn on encoder is conneted to on Arduino
#define PUSHSWITCH 4  //Pin that the the push switch action is attached to
#define SW1 5
#define SW2 6
#define SW3 7
#define SW4 8


#define LONGPRESS 500 //Milliseconds required for a push to become a "long press"
#define SHORTPRESS 0  //Milliseconds required for a push to become a "short press"
#define DEBOUNCETIME 250  //Milliseconds of delay to ensure that the push-switch has debounced
#define BACKTOTUNETIME 5000 //Milliseconds of idle time before exiting a button push state

Rotary r = Rotary(ROTARYLEFT, ROTARYRIGHT); //This sets up the Rotary Encoder including pin modes.

//These are used for the OLED Screen
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)

int underBarX;  //This is the global X value that set the location of the underbar
int underBarY;  //This is the global Y value that set the location of the underbar

//THis is for the second line of text. Use either Display IF Frequency, or a banner. Your choice
//Might as well put the banner here, because you'll change it for your callsign!
//Just raise a pint in my direction when you tell everyone that you wrote this :-)

//#define DISPLAYIFFREQUENCY //This displays the IFFREQ. 
#define USEBANNER //bit of fun with a banner message, comment out if you want to turn it off
#define BANNERMESSAGE "SI4735 RADIO V1.0"
#define BANNERX 0
#define BANNERY 25

//The following values need to be positive as they are used as "unsigned long int" types in the EEPROM routines

//#define IFFREQ 455000 //IF Frequency - offset between displayed and produced signal
//#define IFFERROR 1500 //Observed error in BFO
#define IFFREQ 0 //IF Frequency - offset between displayed and produced signal
#define IFFERROR 0 //Observed error in BFO
#define MAXFREQ 30000000  //Sets the upper edge of the frequency range (30Mhz)
#define MINFREQ 100000    //Sets the lower edge of the frequency range (100Khz)


//////////////////////////////////////////////////////////////////////////////////////////////////////

//Defaults for the code writing to the EEPROM
#define SIGNATURE 0xAABB //Used to check if the EEPROM has been initialised
#define SIGLOCATION 0    //Location where SIGNATURE IS STORED
#define FREQLOCATION 4   //Location where Current Frequency is stored
#define STEPLOCATION 8   //Location where Current Step size is stored

#define DEFAULTFREQ 7000000 //Set default frequency to 7Mhz. Only used when EEPROM not initialised
#define DEFAULTSTEP 1000    //Set default tuning step size to 1Khz. Only used when EEPROM not initialised
#define UPDATEDELAY 5000    //When tuning you don't want to be constantly writing to the EEPROM. So wait
                            //For this period of stability before storing frequency and step size
#define BFORANGE 16000      //Hz before resetting maintuning on the SI4735 BFO
                                                  
//////////////////////////////////////////////////////////////////////////////////////////////////////

long tuneStep;        //global for the current increment - enables it to be changed in interrupt routines
long ifFreq = IFFREQ+IFFERROR; //global for the receiver IF. Made variable so it could be manipulated by the CLI for instance
double rx;            //global for the current receiver frequency
long bfo = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); //global handle to the display
bool fast = false;

#ifndef UPLOADPATCH

unsigned long int lastMod; //This records the time the last modification was made. 
                           //It is used to know when to confirm the EEPROM update
bool freqChanged = false;  //This is used to know if there has been an update, if so
                           //It is a candidate for writing to EEPROM if it was last done long enough ago

void setup()
{
  Serial.begin(9600);
  
  while (!Serial);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 failed"));
    for (;;); // Don't proceed, loop forever
  }

  pinMode(PUSHSWITCH, INPUT_PULLUP);
  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);
  pinMode(SW4, INPUT_PULLUP);
  
  Wire.setClock(10000);   // I2C Speed available
  displaybanner();        //Show a banner message to the world
  testsmeter();           //Swing the smeter to check it's functioning
  readDefaults();         //check EEPROM for startup conditions
  setTuneStepIndicator(); //set up the X&Y for the step underbar 
  displayFrequency(rx);   //display the frequency on the OLED

//Set up the radio
  initialiseradio();
  sendFrequency(rx); //send the command to the 9850 Module, adjusted up by the IF Frequency

}


/////////////////////////////////////////

 void loop ()
 {
  int result = r.process();       //This checks to see if there has been an event on the rotary encoder.
  updatesmeter();
  if (result)
  {
    freqChanged=true; //used to check the EEPROM writing            
    lastMod=millis(); //used to check the EEPROM writing
    printStatus();
    
    //Increment or decrement the frequency by the tuning step depending on direction of movement.
    if (result == DIR_CW) {
        rx+=tuneStep;
        bfo+=tuneStep;
        if (rx>MAXFREQ) {rx = MAXFREQ;}
        displayFrequency(rx);
        sendFrequency(rx);
      } else {
        rx-=tuneStep;
        bfo-=tuneStep;
        if (rx<MINFREQ) {rx = MINFREQ;}
        displayFrequency(rx);
        sendFrequency(rx);      
      }
  }
  
  
  if (digitalRead(PUSHSWITCH) == LOW) {
    doMainButtonPress();  //process the switch push
  }

  if (digitalRead(SW1) == LOW) {
    doSw1ButtonPress();  //process the switch push
  }
  
  if (digitalRead(SW2) == LOW) {
    doSw2ButtonPress();  //process the switch push
  }
  
  if (digitalRead(SW3) == LOW) {
    doSw3ButtonPress();  //process the switch push
  }
  
  if (digitalRead(SW4) == LOW) {
    doSw4ButtonPress();  //process the switch push
  }
  
  if ((freqChanged) & (millis()-lastMod>UPDATEDELAY) )
  {
    commitEPROMVals();
    freqChanged=false;
  }

 }
#endif //NDEF OF UPLOADPATCH

///////////////////////////////////
bool isLongPress(int button)
{
  long int pressTime = millis();  //reord when we enter the routine, used to determine shortlongPress
  waitStopBounce(button);               //wait until the switch noise has gone
  while (digitalRead(button) == LOW) //Sit in this routine while the button is pressed
  {
    delay(1); //No operation but makes sure the compiler doesn't optimise this code away
  }
  pressTime = millis() - pressTime; //This records the duration of the button press
  
  #ifdef DEBUG
    Serial.print("Button Press Duration (ms) : ");
    Serial.println(pressTime);
  #endif

  if (pressTime > LONGPRESS) return true;
  return false;
}


/////////////////////////////////////
void doMainButtonPress(){
    
    if (isLongPress(PUSHSWITCH)==true) //Check against the defined length of a long press
    {
       //Do long press operations
    }  
    else
    {
      //Do short press operations
      changeFeqStep();
    }
}


///////////////////////////////////////////////////////////////
void doSw1ButtonPress()
{
    waitStopBounce(SW1);
    cycleBandwidth();
}

void doSw2ButtonPress()
{
  if (isLongPress(SW2)==true) //Check against the defined length of a long press
  {
      toggleAGC();
  }
  else
  {
    {
      cycleAGC();
    }
  }
}

void doSw3ButtonPress()
{
}

void doSw4ButtonPress()
{

}

//////////////////////////////////////////////////////////////////////////

void changeFeqStep()
{
  
  unsigned long int pauseTime=millis(); //record when we start this operation
  
  while(digitalRead(PUSHSWITCH)==HIGH) //This stays in this routine until the button is pressed again to exit.
  {
    int result = r.process();
    if (result)
    {
      pauseTime=millis();               //update the timer to show that we've taken action
      
      if (result == DIR_CW) {
          if (tuneStep>1)  { 
            if (tuneStep==1000) {tuneStep=500;}
            else{
              if (tuneStep==500) {tuneStep=100;}
              else
                {
                  tuneStep=tuneStep/10;     
                }
            }
          }
      } else {
          if (tuneStep<10000000)  {
            if (tuneStep==100) {tuneStep=500;}
            else{
              if (tuneStep==500) {tuneStep=1000;}
              else
                {
                  tuneStep=tuneStep*10;     
                }
            }
          }
      }
      setTuneStepIndicator();
      displayFrequency(rx);     
    }
    

    //If no input for moving the dial step then just go back to normal
    //There is a possible - but unlikely - scenario that the button is pressed as this timeout occurs, that would result in
    //bouncy switch condition, hence the debounce requirement
    if (millis()-pauseTime > BACKTOTUNETIME) {
                 waitStopBounce(PUSHSWITCH);
                 return;
    } 
  }

  
  //make sure that the swith has stopped bouncing before returning to the main routine.
  //There is a bug possible here if we don't wait for the release before returning
  //Possible that a long press on the way out of this routine could see you return here
  //due to switch bounce, which would appear to the user that the routine didn't exit
  //also possible to go accidently into a long-press scenario
  
  waitStopBounce(PUSHSWITCH);
 
  while (digitalRead(PUSHSWITCH)==LOW) //to avoid exit bug when the user keeps the button pressed for a long period
  {
    delay(1);                 
  }
  
  waitStopBounce(PUSHSWITCH);                            
}


///////////////////////////////////////////////////////////

////////
void printStatus(void)
{
   Serial.print("RADIO RX Freq = ");
   Serial.print(si4735.getFrequency());
   Serial.print(" RSSI = ");
   si4735.getCurrentReceivedSignalQuality();
   Serial.print(si4735.getCurrentRSSI());
   Serial.print(" SNR = ");
   Serial.print(si4735.getCurrentSNR());   
   Serial.println();
}


///////////////////////////////////////////////////////////
#define AM_FUNCTION 1
#define RESET_PIN 12
#define LSB 1
#define USB 2

uint8_t bandwidthIdx = 2;
const char *bandwitdth[] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};

bool disableAgc = true;
bool avc_en = true;

uint8_t currentAGCAtt = 0;
uint8_t rssi = 0;


//////////////////////////////////////////////////////
void initialiseradio()
{
    int16_t si4735Addr = si4735.getDeviceI2CAddress(RESET_PIN);
  if ( si4735Addr == 0 ) {
    Serial.println("Si473X not found!");
    Serial.flush();
    while (1);
  } else {
#ifdef DEBUG
    Serial.print("The Si473X I2C address is 0x");
    Serial.println(si4735Addr, HEX);
#endif

  si4735.setup(RESET_PIN, AM_FUNCTION);
  loadSSB();
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(MINFREQ/1000,MAXFREQ/1000,getCurrentFreq(),1,USB);
  displayFrequency(rx);
  si4735.setVolume(60);
  Serial.print("RX Freq = ");
  Serial.println(si4735.getFrequency());
  si4735.setFrequency(getCurrentFreq());
  }
}

////////////////////////////////////////////////////////////////////////
uint16_t getCurrentFreq(void)
{
  uint16_t newval = rx/1000;
  return newval;
}


////////////////////////////////////////////////////////////////////////

void loadSSB()
{
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  delay(50);
  //si4735.downloadPatch(ssb_patch_content, sizeof ssb_patch_content);
  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2KHz (default); 1=2.2KHz; 2=3KHz; 3=4KHz; 4=500Hz; 5=1KHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  si4735.setSSBConfig(bandwidthIdx, 1, 0, 1, 0, 1);
}

//////////////////////////////////////////////////////

void sendFrequency(long int frequency) {
  if (abs(bfo)>BFORANGE)
  {
    bfo=0;
    si4735.setFrequency(getCurrentFreq());
  } 
  si4735.setSSBBfo(bfo);
}

///////////////////////////////////////////////////////////////

void toggleAGC(void)
{
    disableAgc = !disableAgc;
    // siwtch on/off ACG; AGC Index = 0. It means Minimum attenuation (max gain)
    si4735.setAutomaticGainControl(disableAgc, currentAGCAtt);
}


///////////////////////////////////////////////////////////////

void cycleAGC (void)
{
    if (currentAGCAtt == 0)
        currentAGCAtt = 1;
      else if (currentAGCAtt == 1)
        currentAGCAtt = 5;
      else if (currentAGCAtt == 5)
        currentAGCAtt = 15;
      else if (currentAGCAtt == 15)
        currentAGCAtt = 26;
      else
        currentAGCAtt = 0;
      si4735.setAutomaticGainControl(1, currentAGCAtt);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

void cycleBandwidth(void)
{
  bandwidthIdx++;
    if (bandwidthIdx > 5)
      bandwidthIdx = 0;
    si4735.setSSBAudioBandwidth(bandwidthIdx);
    // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
    if (bandwidthIdx == 0 || bandwidthIdx == 4 || bandwidthIdx == 5)
      si4735.setSBBSidebandCutoffFilter(0);
    else
      si4735.setSBBSidebandCutoffFilter(1);
}

//////////////////////////////////////////////////////////////////////////

void updatesmeter(void)
{
   int sigStrength;
   si4735.getCurrentReceivedSignalQuality();
   sigStrength=si4735.getCurrentRSSI();
   analogWrite(9,sigStrength*2); 
}

//////////////////////////////////////////////////////////////////////////

void testsmeter(void)
{
  /*test analogue Meter*/
  for (int a=0;a<255;a++)
  {
      analogWrite(9,a);
      delay(10);
  }
  delay(300);
  for (int a=255;a>0;a--)
  {
      analogWrite(9,a);
      delay(10);
  }
 }


//////////////////////////////////////////////////////////////////////////////////////////////////////

void displaybanner(void)
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2); // Draw 2X-scale text
  display.setCursor(10, 0);
  display.println(F("Si4735"));
  display.setCursor(BANNERX, BANNERY);
  display.setTextSize(1); // Draw 1X-scale text
  display.println(F("Gareth Davies"));
  display.display(); 

}

void displayFrequency(long int freq)
{

  //Decompose into the component parts of the frequency.
  //long int hz = (long)currentFrequency * 1000 + currentBFO;
  long int hz=freq;
  long int millions = int(hz / 1000000);
  long int hundredthousands = ((hz / 100000) % 10);
  long int tenthousands = ((hz / 10000) % 10);
  long int thousands = ((hz / 1000) % 10);
  long int hundreds = ((hz / 100) % 10);
  long int tens = ((hz / 10) % 10);
  long int ones = ((hz / 1) % 10);

#ifdef DEBUG
  Serial.print(" HZ=");
  Serial.println(hz);
  Serial.print(millions);
  Serial.print(".");
  Serial.print(hundredthousands);
  Serial.print(tenthousands);
  Serial.print(thousands);
  Serial.print(".");
  Serial.print(hundreds);
  Serial.print(tens);
  Serial.print(ones);
  Serial.println();
#endif
  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  if (millions < 10)
  {
    display.print("0");
  }
  display.setTextSize(2); // Draw 2X-scale text
  display.print(millions);
  display.print(".");
  display.print(hundredthousands);
  display.print(tenthousands);
  display.print(thousands);
  display.setTextSize(1); // Draw 1X-scale text
  display.print(".");
  display.print(hundreds);
  display.print(tens);
  display.print(ones);
  display.setCursor(underBarX, underBarY);
  display.print("-");
#ifdef USEBANNER
  display.setCursor(BANNERX, BANNERY);
  display.print(BANNERMESSAGE);
#endif

#ifdef DISPLAYIFFREQUENCY
  display.setCursor(BANNERX, BANNERY);
  display.print(" IF = ");
  display.print(ifFreq / 1000);
  display.print(".");
  display.print(ifFreq % 1000);
#endif
  display.display();      // Show initial text
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void setTuneStepIndicator()
//This sets up the underbar X & Y locations based on the 
//value of tuneStep, which. Underlines the frequency display
//Values were found by trial and error using the CLI                 
{
    underBarY = 15;
    if (tuneStep==10000000) underBarX=13;
    if (tuneStep==1000000) underBarX=22;
    if (tuneStep==100000) underBarX=48;
    if (tuneStep==10000) underBarX=60;
    if (tuneStep==1000) underBarX=72;
    if (tuneStep<1000) underBarY=7;
    if (tuneStep==100) underBarX=88;
    if (tuneStep==500) underBarX=88;
    if (tuneStep==10) underBarX=95;
    if (tuneStep==1) underBarX=100;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

//Removes bounce on the input switch

//Simple delay version
void waitStopBounce(int pin)
{
  long int startTime = millis();
  while (millis() - startTime < DEBOUNCETIME)
  {
    delay(1);
  }
}

////////////////////////////////////////////////////////////////////////////////////////

void readDefaults()
{
  if (readEPROM(SIGLOCATION) != SIGNATURE)
    {
       //Means that there has not been any initialised sequence stored in EEPROM yet
       //Comes from a virgin processor, or a change in the SIGNATURE
        rx=DEFAULTFREQ;
        tuneStep=DEFAULTSTEP;
        setTuneStepIndicator();
        displayFrequency(rx);
    }
    else
    {
      readEPROMVals();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void readEPROMVals()
{
      rx=readEPROM(FREQLOCATION);
      tuneStep=readEPROM(STEPLOCATION);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void commitEPROMVals()
{
      writeEPROM(SIGLOCATION,SIGNATURE);
      writeEPROM(FREQLOCATION,rx);
      writeEPROM(STEPLOCATION,tuneStep);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void writeEPROM(int addr, unsigned long int inp)
{
  byte lsb=inp;
  byte msb=inp>>8;
  byte mmsb=inp>>16;
  byte mmmsb=inp>>24;
  EEPROM.update(addr,lsb);
  EEPROM.update(addr+1,msb);
  EEPROM.update(addr+2,mmsb);
  EEPROM.update(addr+3,mmmsb);  
#ifdef DEBUG
  Serial.print("EEPROM LOC:");
  Serial.print(addr);
  Serial.print(" Write = ");
  Serial.println(inp);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned long int readEPROM(int addr)
{
  byte lsb=EEPROM.read(addr);
  byte msb=EEPROM.read(addr+1);
  byte mmsb=EEPROM.read(addr+2);
  byte mmmsb=EEPROM.read(addr+3);
  unsigned long int OP=mmmsb;
  OP = (OP<<8);
  OP = OP|mmsb;
  OP = (OP<<8);
  OP = OP|msb;
  OP = (OP<<8);
  OP = OP|lsb;
#ifdef DEBUG
  Serial.print("EEPROM LOC:");
  Serial.print(addr);
  Serial.print(" Read = ");
  Serial.println(OP);
#endif
  return OP;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
//THIS IS CODE FOR BRANCHING OFF TO UPLOAD STATIC CODE FOR THE SSB EEPROM PATCH

#ifdef UPLOADPATCH //ONLY IF THIS IS SET.
void setup()
{
  Serial.begin(9600);
  while (!Serial);
  Serial.flush();
  Serial.println();
  Serial.println("\nUploading EEPROM MODE");
  int16_t si4735Addr = si4735.getDeviceI2CAddress(RESET_PIN);
  if ( si4735Addr == 0 ) {
    Serial.println("Si473X not found!");
    Serial.flush();
    while (1);
  } else {
    Serial.print("The Si473X I2C address is 0x");
    Serial.println(si4735Addr, HEX);
  }
  Serial.flush();
  si4735.uploadPatchToEeprom();
}

void loop() {}
#endif

////////////////////////////////////////////////ABOVE HERE IS CODE FOR LOADING AN EEPROM WITH SSB PATCH
