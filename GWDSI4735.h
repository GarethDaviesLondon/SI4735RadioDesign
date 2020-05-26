#include "SI4735.h"
#define UPLOADPATCH


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

    void GWDSI4735::eepromReadBlock(uint8_t i2c_address, uint16_t offset, uint8_t  * pData, uint8_t blockSize);
    si4735_eeprom_patch_header GWDSI4735::downloadPatchFromEeprom(int eeprom_i2c_address);
    void GWDSI4735::waitToSend();
  
  protected:

#ifdef UPLOADPATCH
    bool GWDSI4735::checkDataWroteOK(const uint8_t  * pData, uint16_t offset, uint8_t datalength);
    void eepromWriteHeader(void);
    void eepromWritePatch(void);
    void eepromWrite(uint8_t i2c_address, uint16_t offset, uint8_t data);
    void eepromWriteBlock(uint8_t i2c_address, uint16_t offset, uint8_t const * pData, uint8_t blockSize);
#endif
  
};
