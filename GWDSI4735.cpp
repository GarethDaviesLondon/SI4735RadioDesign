#include "GWDSI4735.h"

//#define UPLOADPATCH

//////////////////////////////////
//These were used to debug the programming of the EEPROM when tracking a particulary obscure bug....
//#define DEBUG
//#define DEBUGDATA
//#define DEBUGHEADER
//#define DEBUGHEADERDATA
//#define DEBUGPATCH
//#define DEBUGPATCHDATA
#define DEBUGDEBUGSCREENWIDTH 8        //USED FOR FORMATTING DEBUGGING OUTPUT
//////////////////////////////////

#define EEPROM_I2C_ADDR 0x50 // You might need to change this value

//////////////////////////
//const uint8_t content_id[] = "SI4735-D60-init-JUNE2020-V1"; //BUG INDUCING STRING
// THIS CREATES AN EEPROM WRITE BUG IN LOCATIONS 0x40 -> 0x45 For reasons I don't understand.
// Changing the value by 2 digits cures it.
//////////////////////////

const uint8_t content_id[] = "SI4735-D60-init-JUNE2020-V1.1"; //IF you change the number of characters here you will have problems!
const uint8_t size_id = sizeof content_id;

#ifdef DEBUG
const uint8_t content_id_test[] = "SI4735-D60-init-JUNE2020-V2";
#endif


#ifdef UPLOADPATCH //Set in GWDSI4735.h
#include "patch_init.h";


//Store the SSB Patch into EEPROM

///////////////////////////////////////////////////////////////
int GWDSI4735::uploadPatchToEeprom(void)
{
  Serial.println("Programming EEPROM");
  bool success=true;
  eepromWriteHeader();
  eepromWritePatch();
 
  if (!checkHeaderWroteOK(content_id,0,size_id))
  {
    success=false;
    #ifdef DEBUG
      Serial.println("[FAIL] Unexepected Error has been Detected in Header Information");
    #endif //DEBUG
  }

#ifdef DEBUG //Second surity check
  else {
    Serial.println("[PASS] No Error was detected in re-called Header Information");
  }
  Serial.println("\n\nDebug mode second pass - checking comparison of stored data with known error to check it is detected");
  if (!checkHeaderWroteOK(content_id_test,0,sizeof content_id_test))
  {
      Serial.println("[PASS] Deliberate Error was Detected in Header Information");
  } else {
      Serial.println("[FAIL] Deliberate Error was NOT Detected in Header Information"); 
  }
#endif //DEBUG

  if (checkPatchWroteOK(ssb_patch_content,size_id+2,sizeof ssb_patch_content))
  {

#ifdef DEBUG
      Serial.println("[PASS] Read MATCHES Patch Data");
#endif //DEBUG

  } else {
    success=false;
#ifdef DEBUG
      Serial.println("[FAIL] Read Data DOES NOT MATCH Patch Data");
#endif //DEBUG
  }
  if (success==true){
      Serial.println("EEPROM has been programmed and passed testing");
  }else{
      Serial.println("EEPROM has NOT BEEN PROGRAMMED OR FAILED TESTING");
  }
}

///////////////////////////////////////////////////////////////
void GWDSI4735::eepromWriteHeader(void)
{
   #ifdef DEBUGHEADER
    Serial.print("\nHEADER WRITER EXECUTING\nEEPROM content identifier is being sent for storage\n"); 
   #endif
   #ifdef DEBUGHEADERDATA
    int widthcheck=0;
    for (int a = 0; a< size_id; a++)
    {
      Serial.print("0x");
      Serial.print(content_id[a],HEX);
      Serial.print(" ");
      widthcheck++;
      if(widthcheck==DEBUGSCREENWIDTH){widthcheck=0;Serial.println();}
    }
    Serial.println();
  #endif
  eepromWriteBlock(EEPROM_I2C_ADDR,0,content_id,size_id);
  uint16_t patch_length = sizeof ssb_patch_content;
#ifdef DEBUGHEADER
    Serial.print("\nPreparing to write the length of the patch data which follows = 0x");
    Serial.print(patch_length,HEX);
    Serial.println();
#endif
  eepromWriteInt(EEPROM_I2C_ADDR,size_id,patch_length);
}


///////////////////////////////////////////////////////////////
void GWDSI4735::eepromWritePatch(void)
{
  register int i, offset,deltaOffset;
  uint8_t content[8];
  offset = size_id+2;
  Serial.print("\nPatch Length is 0x");
  Serial.println(sizeof ssb_patch_content,HEX);
   
 #ifdef DEBUGPATCH
    int widthcheck=0;
    Serial.print("\nEEPROM PATCH DATA To be sent for storage\n");
 #endif
 
    for (deltaOffset = 0; deltaOffset < sizeof ssb_patch_content; deltaOffset += 8)
    {
        for (i = 0; i < 8; i++)
        {
            content[i] = pgm_read_byte_near(ssb_patch_content + (i + deltaOffset));
  #ifdef DEBUGPATCHDATA
            Serial.print("0x");
            Serial.print(content[i],HEX);
            Serial.print(" ");
            widthcheck++;
            if(widthcheck==DEBUGSCREENWIDTH){widthcheck=0;Serial.println();}
  #endif
         }
         eepromWriteBlock( EEPROM_I2C_ADDR,  deltaOffset+offset, content, 8);
    }
  #ifdef DEBUGPATCHDATA
    Serial.println();
  #endif


}

///////////////////////////////////////////////////////////////
void  GWDSI4735::eepromWriteBlock(uint8_t i2c_address, uint16_t offset, uint8_t const * pData, uint8_t blockSize)
{
  bytes_to_word16 eeprom;
  eeprom.value = offset;

  Wire.beginTransmission(i2c_address);
  Wire.write(eeprom.raw.highByte); // Most significant Byte
  Wire.write(eeprom.raw.lowByte);  // Less significant Byte
  
 #ifdef DEBUGDATA
  Serial.print("Low-level writing Bytes to EEPROM From Offset ");
  Serial.print(offset,HEX);
  Serial.print(" Transmission Length ");
  Serial.println(blockSize,HEX);
  int widthcheck=0;
#endif

  for (int i = 0; i < blockSize; i++)
  {

#ifdef DEBUGDATA
        Serial.print("0x");
        Serial.print(*pData,HEX);
        Serial.print(" ");
        widthcheck++;
        if(widthcheck==DEBUGSCREENWIDTH){widthcheck=0;Serial.println();}
#endif
        Wire.write(*pData++); //dispatch the data bytes
  }
#ifdef DEBUGDATA
  Serial.println();
#endif
 
  Wire.endTransmission();
  delay(5); 

}


///////////////////////////////////////////////////////////////
void GWDSI4735::eepromWrite(uint8_t i2c_address, uint16_t offset, uint8_t data)
{
  bytes_to_word16 eeprom;
  eeprom.value = offset;
  Wire.beginTransmission(i2c_address);
  // First, you have to tell where you want to save the data (offset is the position).
  Wire.write(eeprom.raw.highByte); // Most significant Byte
  Wire.write(eeprom.raw.lowByte);  // Less significant Byte
  Wire.write(data);                // Writes the data at the right position (offset)
  Wire.endTransmission();
  delay(5);
}



///////////////////////////////////////////////////////////////
void GWDSI4735::eepromWriteInt(uint8_t i2c_address, uint16_t offset, uint16_t data)
{
  bytes_to_word16 eeprom,datatowrite;
  eeprom.value = offset;
  datatowrite.value = data;
  Wire.beginTransmission(i2c_address);
  // First, you have to tell where you want to save the data (offset is the position).
  Wire.write(eeprom.raw.highByte); // Most significant Byte
  Wire.write(eeprom.raw.lowByte);  // Less significant Byte
  Wire.write(datatowrite.raw.highByte);      // Writes the data at the right position (offset)
  Wire.write(datatowrite.raw.lowByte);      // Writes the data at the right position (offset)
  Wire.endTransmission();
  delay(5);
}


///////////////////////////////////////////////////////////////
bool GWDSI4735::checkPatchWroteOK(const uint8_t  * pData, uint16_t offset, uint8_t datalength)
{
  register int i,deltaOffset;
  uint8_t content;
  uint8_t readEpromContent[8];
  bytes_to_word16 eeprom;
 

  for (deltaOffset = 0; deltaOffset < sizeof ssb_patch_content; deltaOffset += 8)
    {
     eepromReadBlock(EEPROM_I2C_ADDR,offset+deltaOffset,readEpromContent,8);     
        for (i = 0; i < 8; i++)
        {
            content = pgm_read_byte_near(ssb_patch_content + (i + deltaOffset));
  #ifdef DEBUGPATCHDATA
            Serial.print("Patch value 0x");
            Serial.print(content,HEX);
            Serial.print(" Compares to EEPROM VALUE 0x");
            Serial.print(readEpromContent[i],HEX);
            if (readEpromContent[i]==content)
            {
               Serial.println(" - OK");
            } else {
              Serial.println(" - Fail");
            }
  #endif
            if (readEpromContent[i]!=content) return false;
         }
         Wire.endTransmission();
    }
    return true;
}

#endif //PATCHUPLOAD - THE CODE ABOVE HERE IS ONLY USED TO PROGRAMME THE EEPROM SO IS COMPILED ONLY WHEN NEEDED

bool GWDSI4735::checkHeaderWroteOK(const uint8_t  * pData, uint16_t offset, uint8_t datalength)
{
    uint8_t read_id[datalength];;
    
  #ifdef DEBUGHEADERDATA
    Serial.print("\nCHECKING BY READING - THE DATA WE EXPECT BACK = \n");
    int widthcheck=0;
    for (int a = 0; a< datalength; a++)
    {
      Serial.print("0x");
      Serial.print(pData[a],HEX);
      Serial.print(" ");
      widthcheck++;
      if(widthcheck==DEBUGSCREENWIDTH){widthcheck=0;Serial.println();}
    }
    Serial.println();
  #endif
  
    eepromReadBlock(EEPROM_I2C_ADDR,offset,read_id,datalength);
         
  #ifdef DEBUGHEADERDATA
    Serial.print("\nEEPROM DATA SENT BACK FOR COMPARISON = \n");
    widthcheck=0;
    for (int a = 0; a< datalength; a++)
    {
      Serial.print("0x");
      Serial.print(read_id[a],HEX);
      Serial.print(" ");
      widthcheck++;
      if(widthcheck==DEBUGSCREENWIDTH){widthcheck=0;Serial.println();}
    }
    Serial.println();
  #endif

  uint16_t patchSizeExpected = eepromReadInt(EEPROM_I2C_ADDR,size_id);
  #ifdef DEBUGHEADER
    Serial.print("\nChecking Expected Patch Size recorded = 0x");
    Serial.println(patchSizeExpected,HEX);
  #endif

#ifdef PATCHUPLOAD
  if (patchSizeExpected != sizeof ssb_patch_content) {return false;}
  #ifdef DEBUGHEADER
    Serial.print("Passed Expected Patch Size Check\n");
  #endif
#endif

  for (int a = 0; a< datalength; a++)
    {
      if (read_id[a] != pData[a]) return false;
    }
  return true;
}


///////////////////////////////////////////////////////////////
void  GWDSI4735::eepromReadBlock(uint8_t i2c_address, uint16_t offset, uint8_t  * pData, uint8_t blockSize)
{
  bytes_to_word16 eeprom;
  eeprom.value = offset;
  
  Wire.beginTransmission(i2c_address);
  Wire.write(eeprom.raw.highByte); // Most significant Byte
  Wire.write(eeprom.raw.lowByte);  // Less significant Byte
  Wire.endTransmission();
  Wire.requestFrom(i2c_address,blockSize);
  
#ifdef DEBUGDATA
  uint8_t data;
  Serial.print("\nEEPROM raw low level reading blocks from EEPROM at offset ");
  Serial.print(offset,HEX);
  Serial.print(" Requesting Data block size ");
  Serial.println(blockSize,HEX);
  int widthcheck=0;
  for (int i = 0; i < blockSize; i++)
  {
    data=Wire.read(); //capture the data bytes
    *pData++=data;
      Serial.print("0x");
      Serial.print(data,HEX);
      Serial.print(" ");
      widthcheck++;
      if(widthcheck==DEBUGSCREENWIDTH){widthcheck=0;Serial.println();}
    }
    Serial.println();
 #else
  for (int i = 0; i < blockSize; i++)
  *pData++=Wire.read(); //capture the data bytes
 #endif
  Wire.endTransmission();
  delay(5);

}


///////////////////////////////////////////////////////////////


uint16_t GWDSI4735::eepromReadInt(uint8_t i2c_address, uint16_t offset)
{
  bytes_to_word16 eeprom,datatowrite;
  eeprom.value = offset;
  
  Wire.beginTransmission(i2c_address);
  Wire.write(eeprom.raw.highByte); // offset Most significant Byte
  Wire.write(eeprom.raw.lowByte); // offset Less significant Byte
  Wire.endTransmission();
  Wire.requestFrom(i2c_address, 2);
  eeprom.raw.highByte=Wire.read();
  eeprom.raw.lowByte=Wire.read();
  return (eeprom.value);
}

  
///////////////////////////////////////////////////////////////
void GWDSI4735::downloadPatchFromEeprom(void)
{
    #ifdef DEBUG
      Serial.println("Hello from GWDSI4735::downloadPatchFromEeprom"); //Checks I am using my version!
    #endif
  
    uint8_t bufferAux[8];
    int offset, i;

    // Check the EEPROM patch header information
    if (!checkHeaderWroteOK(content_id,0,size_id))
    {
      Serial.println("Error in EEPROM Data Header does not match - Abort Patch Load");
      return;
    }

    //Get the length of the patch data
    int patchLength=eepromReadInt(EEPROM_I2C_ADDR,size_id);
    
    #ifdef DEBUG
    Serial.print("Header OK - about to transfer data, length = 0x");
    Serial.println(patchLength,HEX);
    #endif
    
    // Transferring patch from EEPROM to SI4735 device
    offset = size_id+2;
    for (i = 0; i < patchLength; i += 8)
    {
          #ifdef DEBUG
            Serial.print("*");
            if ((i%512==0)&(i>0)) {Serial.print(" 0x");Serial.print(i,HEX);Serial.println();}
          #endif  
        // Reads patch content from EEPROM
        Wire.beginTransmission(EEPROM_I2C_ADDR);
        Wire.write((int)offset >> 8);   // header_size >> 8 wil be always 0 in this case
        Wire.write((int)offset & 0XFF); // offset Less significant Byte
        Wire.endTransmission();

        Wire.requestFrom(EEPROM_I2C_ADDR, 8);
        for (int j = 0; j < 8; j++)
        {
            bufferAux[j] = Wire.read();
        }

        Wire.beginTransmission(deviceAddress);
        Wire.write(bufferAux, 8);
        Wire.endTransmission();
        waitToSend();
        
        uint8_t cmd_status;       
        Wire.requestFrom(deviceAddress, 1);
        cmd_status = Wire.read();
        // The SI4735 issues a status after each 8 byte transfered.Just the bit 7(CTS)should be seted.if bit 6(ERR)is seted, the system halts.
        if (cmd_status != 0x80)
        {
            Serial.println("Error in EEPROM writing data to SI4735 - Abort Patch Load");
            return;
        }
        
        offset += 8;                                 // Start processing the next 8 bytes
    }
    #ifdef DEBUG
        Serial.print("\nFinished\n");
    #endif
}

///////////////////////////////


void GWDSI4735::ssbPowerUp()
{
    waitToSend();
    Wire.beginTransmission(deviceAddress);
    Wire.write(POWER_UP);
//    Wire.write(0b00010001); // Set to AM/SSB, disable interrupt; disable GPO2OEN; boot normaly; enable External Crystal Oscillator  .
    Wire.write(0b00010000); // Set to AM/SSB, disable interrupt; disable GPO2OEN; boot normaly; enable External Crystal Oscillator  .
    Wire.write(0b00000101); // Set to Analog Line Input.
    Wire.endTransmission();
    delayMicroseconds(2500);

    powerUp.arg.CTSIEN = 0;          // 1 -> Interrupt anabled;
    powerUp.arg.GPO2OEN = 0;         // 1 -> GPO2 Output Enable;
    powerUp.arg.PATCH = 0;           // 0 -> Boot normally;
    //powerUp.arg.XOSCEN = 1;          // 1 -> Use external crystal oscillator;
    powerUp.arg.XOSCEN = 0;          // 0 -> Use external clock reference;
    powerUp.arg.FUNC = 1;            // 0 = FM Receive; 1 = AM/SSB (LW/MW/SW) Receiver.
    powerUp.arg.OPMODE = 0b00000101; // 0x5 = 00000101 = Analog audio outputs (LOUT/ROUT).
}



/////////

int GWDSI4735::setClockFrequency(long int Hz)
{
  uint16_t value=0;
  //There is a control bit D12 to change the clock source pin. 0 (default = RCLK).
  //4Mhz = 32Khz * 125; 
  value=125;
  setProperty(REFCLK_PRESCALE,value);
  value=32000;  
  setProperty(REFCLK_FREQ,value);
 
}

/////

//#define REFCLK_FREQ 0x0201                          //Sets frequency of reference clock in Hz. The range is 31130 to 34406 Hz, or 0 to disable the AFC. Default is 32768 Hz.
//#define REFCLK_PRESCALE 0x0202                      // Sets the prescaler value for RCLK input.
//#define AM_MODE_AFC_SW_PULL_IN_RANGE 0x3104         // Sets the SW AFC pull-in range.
//#define AM_MODE_AFC_SW_LOCK_IN_RANGE 0x3105         // Sets the SW AFC lock-in.
