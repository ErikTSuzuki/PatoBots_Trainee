#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// Bluetooth / Bluepad32 (conforme seu exemplo)
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <uni.h>

// Seus arquivos
#include "motor.h"
#include "sensor_distancia.h"
#include "pinos.h"
#include "controle.h"

// ---------------------------------------------------------
// TASK DO ROBÔ (Sensores e Autônomo)
// ---------------------------------------------------------

ModoRobo modo_atual = MODO_RC;

motor_config_t motor_direito = {
    .pin_in1     = PIN_MOTOR_RIGHT_IN1,
    .pin_in2     = PIN_MOTOR_RIGHT_IN2,
    .pin_pwm     = PIN_MOTOR_RIGHT_PWM,
    .pwm_channel = MOTOR_RIGHT_CANAL
};

motor_config_t motor_esquerdo = {
    .pin_in1     = PIN_MOTOR_LEFT_IN1,
    .pin_in2     = PIN_MOTOR_LEFT_IN2,
    .pin_pwm     = PIN_MOTOR_LEFT_PWM,
    .pwm_channel = MOTOR_LEFT_CANAL
};


void robot_logic_task(void *pvParameters) {
    printf("[Robo] Task de combate iniciada!\n");
    
    while (1) {
        if (modo_atual == MODO_AUTONOMO) {
            int raw = ler_sensor_raw(ADC_CHANNEL_7);
            int cm  = raw_para_cm(raw);

            if (cm < 25 && cm != 999) {
                motor_set_speed(&motor_direito, 1023);
                motor_set_speed(&motor_esquerdo, 1023);
            } else {
                motor_set_speed(&motor_direito, 500);
                motor_set_speed(&motor_esquerdo, -500);
            }
        }
        // Delay para o Watchdog do ESP32 não reclamar
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}

// ---------------------------------------------------------
// APP_MAIN (Exatamente como o exemplo que funcionava)
// ---------------------------------------------------------
struct uni_platform* get_my_platform(void);

void app_main(void) {
    // 1. Inicializa Memória (Essencial para o Bluetooth salvar o pareamento)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. Inicializa Hardware do Robô
    motor_init_stby(PIN_STBY);
    motor_init(&motor_direito);
    motor_init(&motor_esquerdo);
    inicializar_sensores();

    // 3. CRIA A TASK DO ROBÔ (Para rodar em paralelo com o Bluetooth)
    xTaskCreate(robot_logic_task, "robot_task", 4096, NULL, 5, NULL);

    // 4. Inicializa Bluetooth (Cópia fiel do seu exemplo)
    btstack_init();
    uni_platform_set_custom(get_my_platform());
    uni_init(0, NULL);
    
    printf("--- Bluetooth Iniciado. Aguardando Xbox... ---\n");
    
    // 5. Entra no loop do BTstack (Bloqueia aqui, mas a task acima continua rodando!)
    btstack_run_loop_execute();
}