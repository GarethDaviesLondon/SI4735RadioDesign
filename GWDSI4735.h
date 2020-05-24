
#include "SI4735.h"

class GWDSI4735 : public SI4735 {
  public:
  int testMethod();
  si4735_eeprom_patch_header GWDSI4735::downloadPatchFromEeprom(int eeprom_i2c_address);
  void GWDSI4735::waitToSend();
};
