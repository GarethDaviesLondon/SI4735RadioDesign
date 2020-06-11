#include "SI4735.h"
//#define UPLOADPATCH


typedef union {
  struct
  {
    uint8_t lowByte;
    uint8_t highByte;
  } raw;
  uint16_t value;
} bytes_to_word16; 



class GWDSI4735 : public SI4735 {
  
  public:
  
#ifdef UPLOADPATCH
    int GWDSI4735::uploadPatchToEeprom(void);
#endif
    int GWDSI4735::setClockFrequency(long int);
    void GWDSI4735::eepromReadBlock(uint8_t i2c_address, uint16_t offset, uint8_t  * pData, uint8_t blockSize);
    void GWDSI4735::downloadPatchFromEeprom(void);
  
  protected:

#ifdef UPLOADPATCH
    bool GWDSI4735::checkPatchWroteOK(const uint8_t  * pData, uint16_t offset, uint8_t datalength);
    void eepromWriteHeader(void);
    void eepromWritePatch(void);
    void eepromWrite(uint8_t i2c_address, uint16_t offset, uint8_t data);
    void eepromWriteBlock(uint8_t i2c_address, uint16_t offset, uint8_t const * pData, uint8_t blockSize);
    void eepromWriteInt(uint8_t i2c_address, uint16_t offset, uint16_t data);
#endif

    bool GWDSI4735::checkHeaderWroteOK(const uint8_t  * pData, uint16_t offset, uint8_t datalength);
    uint16_t eepromReadInt(uint8_t i2c_address, uint16_t offset);
    void GWDSI4735::ssbPowerUp();



  
};
