#include <iikitmini.h>      // Biblioteca base do framework Arduino
#include "util/AdcDmaEsp.h" // Classe para configuração do ADC e DMA
#include "util/jtask.h"

#define RESOLUTION 100           // Número de pontos na senoide em um ciclo completo
#define MEIOSPAN 127.5

// Calcula o tempo entre amostras com base na frequência inicial
#define frequency 100UL                     // Frequência inicial da senoide em Hz  
#define sampleRate (frequency * RESOLUTION) // Número de amostras por segundo que queremos gerar.
#define samplePeriod (1000000UL / sampleRate) // Tempo entre uma amostra e a próxima, em microssegundos (µs). OBS.: 1000000: Número de microssegundos em um segundo (1 segundo = 1.000.000 µs).

uint32_t currentSample = 0;      // Índice da amostra atual
void updateDAC() {
  dacWrite(def_pin_DAC1, uint8_t((MEIOSPAN * sin((2 * PI * currentSample) / RESOLUTION)) + MEIOSPAN)); // Escreve o valor no DAC
  currentSample = (currentSample + 1) % RESOLUTION;
}

void setup()
{
  IIKit.setup();
  adcDmaSetup(ADC1_CHANNEL_0,[](const int16_t *y, size_t ylen){IIKit.WSerial.plot("adcValue", (uint32_t) 1000, y, ylen);}, ADC_WIDTH_BIT_12);
  jtaskAttachFunc(updateDAC, samplePeriod);  
}

void loop()
{
  IIKit.loop();
  adcDmaLoop();
  jtaskLoop();  
}
