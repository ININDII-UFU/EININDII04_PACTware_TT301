//https://docs.espressif.com/projects/arduino-esp32/en/latest/api/timer.html
#include <Arduino.h>    // Biblioteca base do framework Arduino, necessária para funções básicas como Serial e delays.
#include <driver/adc.h> // Biblioteca do ESP-IDF para controle avançado do ADC, fornecendo APIs específicas para o ESP32.

// Configurações
#define NUM_SAMPLES 1024    // Define o tamanho máximo da fila (número de amostras armazenadas).
#define SAMPLE_RATE 2000    // Define a taxa de amostragem, neste caso, 2000 amostras por segundo.
#define ADC_CHANNEL ADC1_CHANNEL_0 // Define o canal do ADC usado. No ESP32, ADC1_CHANNEL_0 corresponde ao GPIO36.

QueueHandle_t adcQueue; // Declaração de uma fila do FreeRTOS para armazenar as amostras do ADC.
// A fila permitirá que dados sejam armazenados e acessados de forma segura entre a ISR e o loop principal.

// Configuração global do timer
hw_timer_t *timer = nullptr;

// Função chamada pelo timer para capturar dados do ADC
void IRAM_ATTR timerCallback() {
    uint16_t adcValue = adc1_get_raw(ADC_CHANNEL); // Lê o valor bruto do ADC para o canal configurado (GPIO36).
    xQueueSendFromISR(adcQueue, &adcValue, nullptr); // Envia o valor do ADC para a fila.
    // xQueueSendFromISR é uma função do FreeRTOS projetada para ser chamada dentro de ISRs.
}

// Configuração inicial para amostragem do ADC
void startSampling() {
    // Configuração do ADC
    adc1_config_width(ADC_WIDTH_BIT_12); // Configura a resolução do ADC para 12 bits (valores de 0 a 4095).
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_12); // Configura a atenuação para 12 dB, permitindo leituras de 0 a 3,3V.

    // Inicializa a fila com capacidade para NUM_SAMPLES elementos, cada um do tamanho de uint16_t.
    adcQueue = xQueueCreate(NUM_SAMPLES, sizeof(uint16_t));
    if (adcQueue == nullptr) { // Verifica se a criação da fila falhou.
        Serial.println("Erro ao criar a fila!"); // Exibe uma mensagem de erro no monitor serial.
        return; // Interrompe a execução da função caso a fila não tenha sido criada.
    }

    // Configuração do timer de hardware
    timer = timerBegin(SAMPLE_RATE); // Inicializa o timer com a frequência de amostragem.
    timerAttachInterrupt(timer, &timerCallback); // Associa a função timerCallback como a ISR do timer.
    timerAlarm(timer,  (80000000 / 80) / SAMPLE_RATE, true, 0);//  // Set alarm to call onTimer (value in microseconds).
}

// Configuração inicial do programa
void setup() {
    Serial.begin(115200); // Inicializa a comunicação serial com baud rate de 115200.
    startSampling(); // Chama a função para configurar o ADC e o timer.
    Serial.println("Iniciando coleta de dados..."); // Mensagem de inicialização exibida no monitor serial.
}

// Loop principal
void loop() {
    uint16_t adcValue; // Declaração de uma variável para armazenar os valores recebidos da fila.
    // Verifica continuamente se há dados na fila
    while (xQueueReceive(adcQueue, &adcValue, 0) == pdTRUE) {
        // Se xQueueReceive retorna pdTRUE, significa que há um item disponível na fila:
        // - O dado é removido da fila e armazenado em adcValue.
        Serial.println(adcValue); // Imprime o valor recebido no monitor serial.
    }
}