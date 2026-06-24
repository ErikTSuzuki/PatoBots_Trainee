/**
 * ir_control.c
 *
 * Decodificador FSM para o controle LE-7701 (Protocolo Sony de 12 bits).
 * Adaptado para ESP-IDF v6.0.1 (Sem travamentos por printf).
 */

#include "ir_control.h"
#include "robot_states.h"
#include "motor.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern motor_config_t* get_motor_direito(void);
extern motor_config_t* get_motor_esquerdo(void);

static const char *TAG = "IR_CTRL";

// Sincronizado exatamente com o seu Tsop.hpp
#define SONY_DELTA          150  // Margem de erro (kDelta)
#define SONY_START_PULSE   2400  // Pulso inicial (kHighInit)
#define SONY_BIT1_PULSE    1200  // Pulso de bit 1 (kBit1)
#define SONY_BIT0_PULSE     600  // Pulso de bit 0 (kBit0)

static int state_ = 0;
static int32_t temp_data_ = -1;
static int32_t data_ = -1;
static int64_t last_edge_time = 0;
static int32_t pulse_time_ = 0;

static int64_t startup_inicio_ms = 0;

static bool isStartSignal(int32_t pulse) {
    return (pulse > (SONY_START_PULSE - SONY_DELTA) && pulse < (SONY_START_PULSE + SONY_DELTA));
}

static bool isBit1(int32_t pulse) {
    return (pulse > (SONY_BIT1_PULSE - SONY_DELTA) && pulse < (SONY_BIT1_PULSE + SONY_DELTA));
}

static bool isBit0(int32_t pulse) {
    return (pulse > (SONY_BIT0_PULSE - SONY_DELTA) && pulse < (SONY_BIT0_PULSE + SONY_DELTA));
}

static void resetState(void) {
    state_ = 0;
    temp_data_ = -1;
}

// Processa as ações baseado no ID mapeado do Tsop.hpp
static void processar_comando(uint32_t comando) 
{
    switch(comando) {
        case 8: // Botão 1 (0x08)
            if (startup_fase == STARTUP_COUNTING) return;
            motor_set_speed(get_motor_direito(),  0);
            motor_set_speed(get_motor_esquerdo(), 0);
            modo_atual        = MODO_DESATIVADO;
            startup_fase      = STARTUP_COUNTING;
            startup_inicio_ms = esp_timer_get_time() / 1000;
            ESP_LOGI(TAG, "[BOTÃO 1] Iniciando contagem de 5s para combate!");
            break;

        case 1032: // Botão 2 (0x0408)
            estrategia_atual = ESTRATEGIA_FRENTE_TOTAL;
            startup_fase     = STARTUP_IDLE;
            ESP_LOGI(TAG, "[BOTÃO 2] Estratégia selecionada: FRENTE TOTAL");
            break;

        case 520: // Botão 3 (0x0208)
            estrategia_atual = ESTRATEGIA_PERCORRER_BORDA;
            startup_fase     = STARTUP_IDLE;
            ESP_LOGI(TAG, "[BOTÃO 3] Estratégia selecionada: PERCORRER BORDA");
            break;

        case 1800: // Botão 8 (0x0708) -> PARADA DE EMERGÊNCIA SUBSTUINDO * e #
            motor_set_speed(get_motor_direito(),  0);
            motor_set_speed(get_motor_esquerdo(), 0);
            modo_atual   = MODO_DESATIVADO;
            startup_fase = STARTUP_IDLE;
            ESP_LOGW(TAG, "[BOTÃO 8] EMERGENCY STOP! Robô Desativado!");
            break;
            
        default:
            // Ignora qualquer outro botão do controle remoto
            break;
    }
}

// Executa o decode idêntico ao do STM32, mas adaptado para C no ESP-IDF
static void decodeProtocol(int32_t pulse)
{
    switch (state_)
    {
    case 0: 
        if (isStartSignal(pulse)) {
            state_ = 1;
            temp_data_ = 0;
        }
        break;

    case 1: 
    case 2: 
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
        if (isBit0(pulse)) {
            temp_data_ <<= 1;
            state_++;
        }
        else if (isBit1(pulse)) {
            temp_data_ = (temp_data_ << 1) | 1;
            state_++;
        }
        else {
            resetState();
        }
        break;

    case 11: 
        if (isBit0(pulse)) {
            temp_data_ <<= 1;
            data_ = temp_data_;
            processar_comando(data_); 
            resetState();
        }
        else if (isBit1(pulse)) {
            temp_data_ = (temp_data_ << 1) | 1;
            data_ = temp_data_;
            processar_comando(data_); 
            resetState();
        }
        else {
            resetState();
        }
        break;

    default:
        resetState();
        break;
    }
}

// ISR — Mede o pulso em LOW e alimenta a FSM
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    int64_t now = esp_timer_get_time();
    int32_t delta_time = (int32_t)(now - last_edge_time);
    last_edge_time = now;

    int pino_nivel = gpio_get_level(PIN_IR_TSOP);

    if (pino_nivel == 1) {
        pulse_time_ = delta_time;
    } 
    else {
        if (delta_time > 15000) { 
            resetState();
        } else {
            decodeProtocol(pulse_time_);
        }
    }
}

// Task que gerencia de forma segura o cronômetro de 5 segundos
static void ir_task(void *arg)
{
    while (1) {
        if (startup_fase == STARTUP_COUNTING) {
            int64_t agora     = esp_timer_get_time() / 1000;
            int64_t decorrido = agora - startup_inicio_ms;

            if (decorrido >= 5000) {
                modo_atual   = MODO_AUTONOMO;
                startup_fase = STARTUP_DONE;
                ESP_LOGI(TAG, "!!! EM COMBATE: MODO AUTÔNOMO ATIVADO !!!");
            } else {
                static int64_t ultimo_log = 0;
                if (agora - ultimo_log >= 1000) {
                    ultimo_log = agora;
                    int restam = (int)((5000 - decorrido + 999) / 1000);
                    ESP_LOGI(TAG, "Lançamento em %ds...", restam);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void ir_control_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_IR_TSOP),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE, 
    };
    gpio_config(&io);

    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Erro ao iniciar o serviço ISR");
    }

    gpio_isr_handler_add(PIN_IR_TSOP, gpio_isr_handler, NULL);
    xTaskCreate(ir_task, "ir_task", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "Controle remoto inicializado via Máquina de Estados no pino %d", PIN_IR_TSOP);
}