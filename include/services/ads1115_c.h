#include <Adafruit_ADS1X15.h>

class ADS1115_c : protected Adafruit_ADS1115
{
public:
    ADS1115_c() : Adafruit_ADS1115() {}
    bool begin() {  ((Adafruit_ADS1115 *)this)->setGain(adsGain_t::GAIN_TWOTHIRDS); return ((Adafruit_ADS1115 *)this)->begin(); }
    uint16_t analogRead(uint8_t channel)
    {
        return ((Adafruit_ADS1115 *)this)->readADC_SingleEnded(channel);
    }
};