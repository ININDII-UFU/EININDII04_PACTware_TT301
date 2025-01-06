#include <Arduino.h>

// Configurações
#define NUMBER_FUNCTION_TIMER 3 // Número máximo de funções registradas
#define FREQUENCY_TIMER 5000   // Frequência timer em Hz - ideal MMC das frequencia

// Variáveis globais
IRAM_ATTR uint32_t intervals[NUMBER_FUNCTION_TIMER];        // Intervalos para as funções
IRAM_ATTR void (*callbacks[NUMBER_FUNCTION_TIMER])();       // Ponteiros para funções de callback
IRAM_ATTR volatile uint32_t tickCounter = 0;               // Contador de ticks
hw_timer_t *timer = nullptr;                               // Ponteiro para o timer de hardware

// Função para anexar uma função callback com sua frequência
void attachFunction(uint8_t index, uint32_t functionFreq, void (*callback)()) {
    if (index >= NUMBER_FUNCTION_TIMER) {
        Serial.println("Erro: Índice fora do limite.");
        return;
    }
    intervals[index] = FREQUENCY_TIMER / functionFreq; // Calcula o intervalo em ticks
    callbacks[index] = callback;                      // Registra a função callback
}

// ISR para o timer
void IRAM_ATTR onTimerISR() {
    tickCounter++; // Incrementa o contador de ticks
    for (int i = 0; i < NUMBER_FUNCTION_TIMER; i++) {
        if (callbacks[i] && (tickCounter % intervals[i] == 0)) {
            callbacks[i](); // Chama a função callback
        }
    }
}

// Função para iniciar o timer
void startTimer(uint8_t timerID = 0) {
    // Configuração do timer de hardware
    timer = timerBegin(FREQUENCY_TIMER); // Inicializa o timer com a frequência de amostragem.
    timerAttachInterrupt(timer, &onTimerISR); // Associa a função timerCallback como a ISR do timer.
    // Repeat the alarm (third parameter) with unlimited count = 0 (fourth parameter).
    timerAlarm(timer,  (80000000 / 80) / FREQUENCY_TIMER, true, 0);//  // Set alarm to call onTimer (value in microseconds).
    timerStart(timer); // Inicia o timer
}

// Exemplo de funções de callback
void IRAM_ATTR function1() {
    Serial.println("Function 1 (2000 Hz)");
}

void IRAM_ATTR function2() {
    Serial.println("Function 2 (2200 Hz)");
}

void IRAM_ATTR function3() {
    Serial.println("Function 3 (2500 Hz)");
}

void setup() {
    Serial.begin(115200);

    // Registra as funções de callback
    attachFunction(0, 2000, function1); // 2000 Hz
    attachFunction(1, 2200, function2); // 2200 Hz
    attachFunction(2, 2500, function3); // 2500 Hz

    // Inicia o timer
    startTimer(2);

    Serial.println("Timer configurado com múltiplas funções.");
}

void loop() {
    // Outras tarefas podem ser realizadas aqui
}