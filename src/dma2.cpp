#include <Arduino.h>
#include <esp_adc/adc_continuous.h> // Biblioteca para modo contínuo com DMA

// Definições
#define ADC_CHANNEL ADC_CHANNEL_0 // Canal ADC (GPIO36)
#define NUM_SAMPLES 1024          // Tamanho do buffer
#define SAMPLE_RATE 2000          // Taxa de amostragem em Hz

// Handle do driver ADC contínuo
adc_continuous_handle_t adc_handle = NULL;

// Configuração do ADC e DMA
void setupADC_DMA() {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = NUM_SAMPLES * sizeof(uint16_t),
        .conv_frame_size = NUM_SAMPLES
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_digi_pattern_config_t channel_pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = ADC_CHANNEL,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12
    };

    adc_continuous_config_t channel_config = {
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1
    };
    channel_config.pattern_num = 1,
    channel_config.adc_pattern = &channel_pattern;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &channel_config));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

uint32_t bytes_read = 0;
uint16_t adc_buffer[NUM_SAMPLES] = {0};

void setup() {
    Serial.begin(115200);
    setupADC_DMA();
}

void loop() {
    esp_err_t ret = adc_continuous_read(adc_handle, (uint8_t *) adc_buffer, sizeof(adc_buffer), &bytes_read, 0);
    if (ret == ESP_OK) {
        for (size_t i = 0; i < bytes_read / sizeof(uint16_t); i++) {
            Serial.println(adc_buffer[i]); // Imprime as amostras
        }
    }
}
