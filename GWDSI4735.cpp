#define DEBUG
#define EEPROM_I2C_ADDR 0x50 // You might need to change this value
#include "GWDSI4735.h"


/*ID CHECK */
const uint8_t content_id[] = "SI4735-D60-init-JUNE2020-V1";
#ifdef DEBUG
const uint8_t content_id_test[] = "SI4735-D60-init-JUNE2020-V2";
#endif

const uint16_t size_id = sizeof content_id;


#ifdef UPLOADPATCH //Set in GWDSI4735.h
#include "patch_init.h";
  const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h

//Store the SSB Patch into EEPROM
int GWDSI4735::uploadPatchToEeprom(void)
{
  //Write Header
  eepromWriteHeader();
  if (!checkHeaderOK(content_id))
  {
    #ifdef DEBUG
      Serial.println("[FAIL] Unexepected Error has been Detected in Header Information");
    #endif
  }
  #ifdef DEBUG
  if (!checkHeaderOK(content_id_test))
  {
      Serial.println("[PASS] Deliberate Error was Detected in Header Information");
  } else {
      Serial.println("[FAIL] Deliberate Error was NOT Detected in Header Information"); 
  }
  #endif
}

void GWDSI4735::eepromWriteHeader(void)
{
   eepromWriteBlock(EEPROM_I2C_ADDR,0,content_id,size_id);
   #ifdef DEBUG
    Serial.print("EEPROM CONTENT ID Sent = ");
    for (int a = 0; a< size_id; a++)
    {
      Serial.print("0x");
      Serial.print(content_id[a],HEX);
      Serial.print(" ");
    }
    Serial.println();
  #endif
}

void  GWDSI4735::eepromWriteBlock(uint8_t i2c_address, uint16_t offset, uint8_t const * pData, uint8_t blockSize)
{
  bytes_to_word16 eeprom;
  eeprom.value = offset;

  Wire.beginTransmission(i2c_address);
  Wire.write(eeprom.raw.highByte); // Most significant Byte
  Wire.write(eeprom.raw.lowByte);  // Less significant Byte

  for (int i = 0; i < blockSize; i++)
    Wire.write(*pData++); //dispatch the data bytes
 
  Wire.endTransmission();
  delay(5); 

}

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
#endif

bool GWDSI4735::checkHeaderOK(const uint8_t  * pData)
{
    uint8_t read_id[size_id];;
    eepromReadBlock(EEPROM_I2C_ADDR,0,read_id,size_id); 
  #ifdef DEBUG
    Serial.print("EEPROM CONTENT ID Chek = ");
    for (int a = 0; a< size_id; a++)
    {
      Serial.print("0x");
      Serial.print(read_id[a],HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.print("EEPROM COMPARE ID Chek = ");
    for (int a = 0; a< size_id; a++)
    {
      Serial.print("0x");
      Serial.print(pData[a],HEX);
      Serial.print(" ");
    }
    Serial.println();
  #endif

  for (int a=0;a<size_id;a++){
    if (read_id[a] != pData[a]) return false;
  }
  return true;
}

void  GWDSI4735::eepromReadBlock(uint8_t i2c_address, uint16_t offset, uint8_t  * pData, uint8_t blockSize)
{
  bytes_to_word16 eeprom;
  eeprom.value = offset;
  

  Wire.beginTransmission(i2c_address);
  Wire.write(eeprom.raw.highByte); // Most significant Byte
  Wire.write(eeprom.raw.lowByte);  // Less significant Byte
  Wire.endTransmission();

  Wire.requestFrom(i2c_address,blockSize);
#ifdef DEBUG
  uint8_t data;
  Serial.print("EEPROM CONTENT IS Read = ");
  for (int i = 0; i < blockSize; i++)
  {
    data=Wire.read(); //capture the data bytes
    *pData++=data;
      Serial.print("0x");
      Serial.print(data,HEX);
      Serial.print(" ");
    }
    Serial.println();
 #else
  for (int i = 0; i < blockSize; i++)
  *pData++=Wire.read(); //capture the data bytes
 #endif
  
  Wire.endTransmission();
  delay(5);

}
  

si4735_eeprom_patch_header GWDSI4735::downloadPatchFromEeprom(int eeprom_i2c_address)
{
    #ifdef DEBUG
      Serial.println("Hello from GWDSI4735::downloadPatchFromEeprom"); //Checks I am using my version!
    #endif
    
    si4735_eeprom_patch_header eep;
    const int header_size = sizeof eep;
    uint8_t bufferAux[8];
    int offset, i;

    // Gets the EEPROM patch header information
    Wire.beginTransmission(eeprom_i2c_address);
    Wire.write(0x00); // offset Most significant Byte
    Wire.write(0x00); // offset Less significant Byte
    Wire.endTransmission();
    delay(5);

    // The first two bytes of the header will be ignored.
    for (int k = 0; k < header_size; k += 8) {
        Wire.requestFrom(eeprom_i2c_address, 8);
        for (int i = k; i < (k+8); i++)
        {
            eep.raw[i] = Wire.read();
            #ifdef DEBUG
              Serial.print("Data ");
              Serial.println(eep.raw[i],HEX);
            #endif
        }
    }

    // Transferring patch from EEPROM to SI4735 device
    offset = header_size;
    for (i = 0; i < (int)eep.refined.patch_size; i += 8)
    {
        // Reads patch content from EEPROM
        Wire.beginTransmission(eeprom_i2c_address);
        Wire.write((int)offset >> 8);   // header_size >> 8 wil be always 0 in this case
        Wire.write((int)offset & 0XFF); // offset Less significant Byte
        Wire.endTransmission();

        Wire.requestFrom(eeprom_i2c_address, 8);
        for (int j = 0; j < 8; j++)
        {
            bufferAux[j] = Wire.read();
        }

        Wire.beginTransmission(deviceAddress);
        Wire.write(bufferAux, 8);
        Wire.endTransmission();

        delay(250);

        waitToSend();
        uint8_t cmd_status;
        Wire.requestFrom(deviceAddress, 1);
        cmd_status = Wire.read();
        // The SI4735 issues a status after each 8 byte transfered.Just the bit 7(CTS)should be seted.if bit 6(ERR)is seted, the system halts.
        if (cmd_status != 0x80)
        {
            strcpy((char *) eep.refined.patch_id, "error!");
            return eep;
        }
        offset += 8;                                 // Start processing the next 8 bytes
        delay(250);
    }

    delay(50);
    return eep;
}





/**
 * @ingroup group06 Wait to send command 
 * 
 * @brief  Wait for the si473x is ready (Clear to Send (CTS) status bit have to be 1).  
 * 
 * @details This function should be used before sending any command to a SI47XX device.
 * 
 * @see Si47XX PROGRAMMING GUIDE; AN332; pages 63, 128
 */
void GWDSI4735::waitToSend()
{
    do
    {
        delayMicroseconds(MIN_DELAY_WAIT_SEND_LOOP); // Need check the minimum value.
        Wire.requestFrom(deviceAddress, 1);
    } while (!(Wire.read() & B10000000));
}
