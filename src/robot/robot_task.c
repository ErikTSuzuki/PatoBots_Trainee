#include "robot_task.h"
#include "robot_states.h"
#include "../hardware/motor/motor.h"
#include "../hardware/sensor/sensor_distancia.h"
#include "../utils/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

ModoRobo modo_atual = MODO_RC;

static motor_config_t motor_direito = {
    .pin_in1     = PIN_MOTOR_RIGHT_IN1,
    .pin_in2     = PIN_MOTOR_RIGHT_IN2,
    .pin_pwm     = PIN_MOTOR_RIGHT_PWM,
    .pwm_channel = MOTOR_RIGHT_CANAL
};

static motor_config_t motor_esquerdo = {
    .pin_in1     = PIN_MOTOR_LEFT_IN1,
    .pin_in2     = PIN_MOTOR_LEFT_IN2,
    .pin_pwm     = PIN_MOTOR_LEFT_PWM,
    .pwm_channel = MOTOR_LEFT_CANAL
};

motor_config_t* get_motor_direito(void) {
    return &motor_direito;
}

motor_config_t* get_motor_esquerdo(void) {
    return &motor_esquerdo;
}

void robot_init(void) {
    modo_atual = MODO_RC;
}

void robot_logic_task(void *pvParameters) {
    printf("[Robot] Tarefa de combate iniciada!\n");

    while (1) {
        if (modo_atual == MODO_AUTONOMO) {
            // Lê sensor frontal IR
            int raw = ler_sensor_raw(ADC_CHANNEL_7);
            int cm  = raw_para_cm(raw);

            // Lógica de combate: se inimigo próximo, ataca!
            if (cm < DISTANCIA_ATAQUE_CM && cm != 999) {
                motor_set_speed(&motor_direito, 1023);
                motor_set_speed(&motor_esquerdo, 1023);
            } else {
                // Comportamento alternativo: girar para procurar
                motor_set_speed(&motor_direito, 500);
                motor_set_speed(&motor_esquerdo, -500);
            }
        }
        // Delay para o Watchdog do ESP32 não reclamar
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
