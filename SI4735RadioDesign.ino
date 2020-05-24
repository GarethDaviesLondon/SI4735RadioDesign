#define DEBUG

#include "GWDSI4735.h"
#include "Rotary.h"
#include <Wire.h>             //Needed by the SPI library
#include <Adafruit_GFX.h>     //Used for the OLED display, called by the SSD1306 Library
#include <Adafruit_SSD1306.h> //This is the OLED driver library



//const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h

#define AM_FUNCTION 1
#define RESET_PIN 12

#define LSB 1
#define USB 2

bool disableAgc = true;
bool avc_en = true;

int currentBFO = 0;

// Some variables to check the SI4735 status
uint16_t currentFrequency;
uint8_t currentStep = 1;
uint8_t currentBFOStep = 25;

uint8_t bandwidthIdx = 2;
const char *bandwitdth[] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};


long et1 = 0, et2 = 0;

typedef struct
{
  uint16_t minimumFreq;
  uint16_t maximumFreq;
  uint16_t currentFreq;
  uint16_t currentStep;
  uint8_t currentSSB;
} Band;

Band band[] = {
  {1800, 2000, 1933, 1, LSB},
  {3500, 4000, 3550, 1, LSB},
  {7000, 7500, 7010, 1, LSB},
  {7200, 8000, 7200, 1, LSB},
  {10000, 10500, 10050, 1, USB},
  {14000, 14300, 14015, 1, USB},
  {18000, 18300, 18100, 1, USB},
  {21000, 21400, 21050, 1, USB},
  {24890, 25000, 24940, 1, USB},
  {27000, 27700, 27300, 1, USB},
  {28000, 28500, 28050, 1, USB}
};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int currentFreqIdx = 2;

uint8_t currentAGCAtt = 0;

uint8_t rssi = 0;


GWDSI4735 si4735;

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

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); //global handle to the display
bool fast = false;

void setup()
{
  Serial.begin(9600);
  while (!Serial);


  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 failed"));
    for (;;); // Don't proceed, loop forever
  }

  pinMode(PUSHSWITCH, INPUT_PULLUP);
  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);
  pinMode(SW4, INPUT_PULLUP);


  display.clearDisplay();
  display.display();

#ifdef DEBUG
  Serial.println("Si4735 Arduino Library");
  Serial.println("SSB TEST");
  Serial.println("By PU2CLR");
#endif


  // Gets and sets the Si47XX I2C bus address
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
  }


  si4735.setup(RESET_PIN, AM_FUNCTION);

  // Testing I2C clock speed and SSB behaviour
  // si4735.setI2CLowSpeedMode();     //  10000 (10KHz)
  // si4735.setI2CStandardMode();        // 100000 (100KHz)
   si4735.setI2CFastMode();         // 400000 (400KHz)
  // si4735.setI2CFastModeCustom(500000); // It is not safe and can crash.

  delay(10);

#ifdef DEBUG
  Serial.println("SSB patch is loading...");
  et1 = millis();
#endif

 // loadSSB();
  si4735.downloadPatchFromEeprom(0x50);

#ifdef DEBUG
  et2 = millis();
  Serial.print("SSB patch was loaded in: ");
  Serial.print( (et2 - et1) );
  Serial.println("ms");
#endif

  //delay(100);

  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  //delay(100);
  currentFrequency = si4735.getFrequency();
  displayFrequency();
  si4735.setVolume(60);

}


void loop()
{
  int result = r.process();       //This checks to see if there has been an event on the rotary encoder.
  if (result)
  {
    if (fast == false)
    {
      if (result == DIR_CW)
      {
        currentBFO += currentBFOStep;
        if (currentBFO == 1000)
        {
          currentBFO = 0;
          si4735.frequencyUp();
          band[currentFreqIdx].currentFreq = currentFrequency = si4735.getCurrentFrequency();
        }
        si4735.setSSBBfo(currentBFO);
        displayFrequency();
      } else {
        currentBFO -= currentBFOStep;
        if (currentBFO == -1000)
        {
          currentBFO = 0;
          si4735.frequencyDown();
          band[currentFreqIdx].currentFreq = currentFrequency = si4735.getCurrentFrequency();
        }
        si4735.setSSBBfo(currentBFO);
        displayFrequency();
      }
      delay(100);
    }
    else // We are in fast mode
    {
      if (result == DIR_CW) {
        si4735.frequencyUp();
        band[currentFreqIdx].currentFreq = currentFrequency = si4735.getCurrentFrequency();
        displayFrequency();
      }
      else
      {
        si4735.frequencyDown();
        band[currentFreqIdx].currentFreq = currentFrequency = si4735.getCurrentFrequency();
        displayFrequency();
      }
      delay(100);
    }
  }
  if (digitalRead(PUSHSWITCH) == LOW) {
    doPUSHSWITCHButtonPress();  //process the switch push
  }

  //See if we've pressed the button
  if (digitalRead(SW1) == LOW) {
    doSw1ButtonPress();  //process the switch push
  }
  //See if we've pressed the button
  if (digitalRead(SW2) == LOW) {
    doSw2ButtonPress();  //process the switch push
  }
  //See if we've pressed the button
  if (digitalRead(SW3) == LOW) {
    doSw3ButtonPress();  //process the switch push

  }//See if we've pressed the button
  if (digitalRead(SW4) == LOW) {
    doSw4ButtonPress();  //process the switch push
  }

}

void doPUSHSWITCHButtonPress()
{
  fast = !fast;
}

void doSw1ButtonPress()
{
  {
    waitStopBounce(SW1);
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
}

void doSw2ButtonPress()
{

  long int pressTime = millis();  //reord when we enter the routine, used to determine the length of the button
  //press

  waitStopBounce(SW2);               //wait until the switch noise has gone

  while (digitalRead(SW2) == LOW) //Sit in this routine while the button is pressed
  {
    delay(1); //No operation but makes sure the compiler doesn't optimise this code away
  }

  pressTime = millis() - pressTime; //This records the duration of the button press

#ifdef DEBUG
  Serial.print("Button Press Duration (ms) : ");
  Serial.println(pressTime);
#endif

  if (pressTime > LONGPRESS) //Check against the defined length of a long press
  {
#ifdef DEBUG
    Serial.println("Long Press Detected");
#endif

    disableAgc = !disableAgc;
    // siwtch on/off ACG; AGC Index = 0. It means Minimum attenuation (max gain)
    si4735.setAutomaticGainControl(disableAgc, currentAGCAtt);
  }
  else
  {
#ifdef DEBUG
    Serial.println("Short Press Detected");
#endif

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
  }
}

void doSw3ButtonPress()
{
  long int pressTime = millis();  //reord when we enter the routine, used to determine the length of the button
  //press

  waitStopBounce(SW2);               //wait until the switch noise has gone

  while (digitalRead(SW2) == LOW) //Sit in this routine while the button is pressed
  {
    delay(1); //No operation but makes sure the compiler doesn't optimise this code away
  }

  pressTime = millis() - pressTime; //This records the duration of the button press

#ifdef DEBUG
  Serial.print("Button Press Duration (ms) : ");
  Serial.println(pressTime);
#endif

  if (pressTime > LONGPRESS) //Check against the defined length of a long press
  {
#ifdef DEBUG
    Serial.println("Long Press Detected");
#endif
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
      Serial.println(F("SSD1306 failed"));
      for (;;); // Don't proceed, loop forever
    }
    display.clearDisplay();
    display.display();
    displayFrequency();
  }
  else
  {
#ifdef DEBUG
    Serial.println("Short Press Detected");
#endif
    bandUp();
    displayFrequency();
  }
}

void doSw4ButtonPress()
{
  waitStopBounce(SW4);
  bandDown();
  displayFrequency();
}


/*
void loadSSB()
{
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  delay(50);
  si4735.downloadPatch(ssb_patch_content, size_content);
  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2KHz (default); 1=2.2KHz; 2=3KHz; 3=4KHz; 4=500Hz; 5=1KHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  si4735.setSSBConfig(bandwidthIdx, 1, 0, 1, 0, 1);
}
*/

void displayFrequency()
{

  //Decompose into the component parts of the frequency.
  long int hz = (long)currentFrequency * 1000 + currentBFO;
  long int millions = int(hz / 1000000);
  long int hundredthousands = ((hz / 100000) % 10);
  long int tenthousands = ((hz / 10000) % 10);
  long int thousands = ((hz / 1000) % 10);
  long int hundreds = ((hz / 100) % 10);
  long int tens = ((hz / 10) % 10);
  long int ones = ((hz / 1) % 10);

#ifdef DEBUG
  //This checks the calculation for frequency worked.
  Serial.print("CF=");
  Serial.print(currentFrequency);
  Serial.print(" BFO=");
  Serial.print(currentBFO);
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

void bandUp()
{
  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if (currentFreqIdx < lastBand)
  {
    currentFreqIdx++;
  }
  else
  {
    currentFreqIdx = 0;
  }
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  currentStep = band[currentFreqIdx].currentStep;
  delay(250);
  currentFrequency = si4735.getCurrentFrequency();
  //showStatus();
}

void bandDown()
{
  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if (currentFreqIdx > 0)
  {
    currentFreqIdx--;
  }
  else
  {
    currentFreqIdx = lastBand;
  }
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  currentStep = band[currentFreqIdx].currentStep;
  delay(250);
  currentFrequency = si4735.getCurrentFrequency();
  //showStatus();
}



// Check if exist some command to execute
/*
  if (Serial.available() > 0)
  {
    Serial.println("Key Pressed");
    char key = Serial.read();

    if (key == '>' || key == '.') // goes to the next band
      bandUp();
    else if (key == '<' || key == ',') // goes to the previous band
      bandDown();


    if (key == 'U' || key == 'u')
    { // frequency up
      si4735.frequencyUp();
      delay(250);
      band[currentFreqIdx].currentFreq = currentFrequency = si4735.getCurrentFrequency();
      showFrequency();
    }
    else if (key == 'D' || key == 'd')
    { // frequency down
      si4735.frequencyDown();
      delay(250);
      band[currentFreqIdx].currentFreq = currentFrequency = si4735.getCurrentFrequency();
      showFrequency();
    }

    if (key == 'W' || key == 'w') // sitches the filter bandwidth
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
      showStatus();
    }
    else if (key == '>' || key == '.') // goes to the next band
      bandUp();
    else if (key == '<' || key == ',') // goes to the previous band
      bandDown();
      /*
    else if (key == 'V')
    { // volume down
      si4735.volumeUp();
      showStatus();
    }
    else if (key == 'v')
    { // volume down
      si4735.volumeDown();
      showStatus();
    }
    else if (key == 'B') // increments the bfo
    {
      currentBFO += currentBFOStep;
      si4735.setSSBBfo(currentBFO);
      showBFO();
    }
    else if (key == 'b') // decrements the bfo
    {
      currentBFO -= currentBFOStep;
      si4735.setSSBBfo(currentBFO);
      showBFO();
    }
    else if (key == 'G' || key == 'g') // switches on/off the Automatic Gain Control
    {
      disableAgc = !disableAgc;
      // siwtch on/off ACG; AGC Index = 0. It means Minimum attenuation (max gain)
      si4735.setAutomaticGainControl(disableAgc, currentAGCAtt);
      showStatus();
    }
    else if (key == 'A' || key == 'a') // Switches the LNA Gain index  attenuation
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
      showStatus();
    }
    else if (key == 's') // switches the BFO increment and decrement step
    {
      currentBFOStep = (currentBFOStep == 50) ? 10 : 50;
      showBFO();
    }
    else if (key == 'S') // switches the frequency increment and decrement step
    {
      if (currentStep == 1)
        currentStep = 5;
      else if (currentStep == 5)
        currentStep = 10;
      else
        currentStep = 1;
      si4735.setFrequencyStep(currentStep);
      band[currentFreqIdx].currentStep = currentStep;
      showFrequency();
    }
    else if (key == 'C' || key == 'c') // switches on/off the Automatic Volume Control
    {
      avc_en = !avc_en;
      si4735.setSSBAutomaticVolumeControl(avc_en);
    }
    else if (key == 'X' || key == 'x')
      showStatus();
    else if (key == 'H' || key == 'h')
      showHelp();
  }
  delay(200);
  }

*/

/*
  void showSeparator()
  {
  Serial.println("\n**************************");
  }

*/

/*
  void showHelp()
  {
  showSeparator();
  Serial.println("Type: ");
  Serial.println("U to frequency up or D to frequency down");
  Serial.println("> to go to the next band or < to go to the previous band");
  Serial.println("W to sitch the filter bandwidth");
  Serial.println("B to go to increment the BFO or b decrement the BFO");
  Serial.println("G to switch on/off the Automatic Gain Control");
  Serial.println("A to switch the LNA Gain Index (0, 1, 5, 15 e 26");
  Serial.println("S to switch the frequency increment and decrement step");
  Serial.println("s to switch the BFO increment and decrement step");
  Serial.println("X Shows the current status");
  Serial.println("H to show this help");
  }

*/



/*
  void showStatus()
  {
  showSeparator();
  Serial.print("SSB | ");

  si4735.getAutomaticGainControl();
  si4735.getCurrentReceivedSignalQuality();

  Serial.print((si4735.isAgcEnabled()) ? "AGC ON " : "AGC OFF");
  Serial.print(" | LNA GAIN index: ");
  Serial.print(si4735.getAgcGainIndex());
  Serial.print("/");
  Serial.print(currentAGCAtt);

  Serial.print(" | BW :");
  Serial.print(String(bandwitdth[bandwidthIdx]));
  Serial.print("KHz");
  Serial.print(" | SNR: ");
  Serial.print(si4735.getCurrentSNR());
  Serial.print(" | RSSI: ");
  Serial.print(si4735.getCurrentRSSI());
  Serial.print(" dBuV");
  Serial.print(" | Volume: ");
  Serial.println(si4735.getVolume());
  showFrequency();
  }
*/

/*
  void bandUp()
  {
  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if (currentFreqIdx < lastBand)
  {
    currentFreqIdx++;
  }
  else
  {
    currentFreqIdx = 0;
  }
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  currentStep = band[currentFreqIdx].currentStep;
  delay(250);
  currentFrequency = si4735.getCurrentFrequency();
  //showStatus();
  }

  void bandDown()
  {
  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if (currentFreqIdx > 0)
  {
    currentFreqIdx--;
  }
  else
  {
    currentFreqIdx = lastBand;
  }
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  currentStep = band[currentFreqIdx].currentStep;
  delay(250);
  currentFrequency = si4735.getCurrentFrequency();
  //showStatus();
  }

  /*
   This function loads the contents of the ssb_patch_content array into the CI (Si4735) and starts the radio on
   SSB mode.
*/
