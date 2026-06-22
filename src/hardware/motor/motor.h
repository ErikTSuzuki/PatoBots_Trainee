#ifndef MOTOR_H_
#define MOTOR_H_

#include "driver/gpio.h"
#include "driver/ledc.h"

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================
typedef struct {
    gpio_num_t pin_in1;
    gpio_num_t pin_in2;
    gpio_num_t pin_pwm;
    ledc_channel_t pwm_channel;
} motor_config_t;

// ============================================================================
// PROTÓTIPOS DAS FUNÇÕES
// ============================================================================

/**
 * @brief Inicializa o pino STBY (Standby) da ponte H.
 * @param pin_stby O pino GPIO conectado ao pino STBY da ponte H.
 */
void motor_init_stby(gpio_num_t pin_stby);

/**
 * @brief Inicializa os pinos de controle e o canal PWM para um motor específico.
 * @param motor Ponteiro para a estrutura com a configuração do motor.
 */
void motor_init(motor_config_t *motor);

/**
 * @brief Controla a direção e velocidade do motor.
 * @param motor Ponteiro para o motor que será acionado.
 * @param speed Velocidade desejada de -1023 (ré máxima) até 1023 (frente máxima).
 *              Se speed == 0, o motor aplica um "short-brake" (freio motor ativo).
 */
void motor_set_speed(motor_config_t *motor, int speed);

<<<<<<< HEAD
#endif // MOTOR_H_
=======
#endif // MOTOR_H_
>>>>>>> upstream/main
