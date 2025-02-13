#include <iikitmini.h>       // Biblioteca base do framework Arduino
#include "util/HartSerial.h" // Classe para configuração do ADC e DMA

HartSerial hart;

void setup()
{
  //IIKit.setup();
  hart.begin();
}

void loop()
{
  //IIKit.loop();
  hart.update();
}
