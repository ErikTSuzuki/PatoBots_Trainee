#include "motor.h"
#include "config.h"

// Inicializa o pino STBY global da ponte H
void motor_init_stby(gpio_num_t pin_stby) {
    gpio_set_direction(pin_stby, GPIO_MODE_OUTPUT);
    // Ativa a ponte H (tira do modo de economia de energia)
    gpio_set_level(pin_stby, 1);
}

// Inicializa o hardware (GPIO e LEDC) para um motor
void motor_init(motor_config_t *motor) {
    // 1. Configuração dos pinos de direção como saída
    gpio_set_direction(motor->pin_in1, GPIO_MODE_OUTPUT);
    gpio_set_direction(motor->pin_in2, GPIO_MODE_OUTPUT);

    // Garante que o motor comece parado
    gpio_set_level(motor->pin_in1, 0);
    gpio_set_level(motor->pin_in2, 0);

    // 2. Configuração do Timer do PWM (aplica-se a todos os canais deste timer)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = MOTOR_PWM_MODE,
        .timer_num        = MOTOR_PWM_TIMER,
        .duty_resolution  = MOTOR_PWM_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 3. Configuração do Canal PWM específico para este motor
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = MOTOR_PWM_MODE,
        .channel        = motor->pwm_channel,
        .timer_sel      = MOTOR_PWM_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = motor->pin_pwm,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

// Lógica de controle de velocidade e direção
void motor_set_speed(motor_config_t *motor, int speed) {
    // Trava de segurança para não estourar a resolução de 10 bits
    if (speed > 1023) speed = 1023;
    if (speed < -1023) speed = -1023;

    if (speed > 0) {
        // Rotação no sentido Horário (Frente)
        gpio_set_level(motor->pin_in1, 1);
        gpio_set_level(motor->pin_in2, 0);
        ledc_set_duty(MOTOR_PWM_MODE, motor->pwm_channel, speed);

    } else if (speed < 0) {
        // Rotação no sentido Anti-horário (Ré)
        gpio_set_level(motor->pin_in1, 0);
        gpio_set_level(motor->pin_in2, 1);
        ledc_set_duty(MOTOR_PWM_MODE, motor->pwm_channel, -speed);

    } else {
        // Parada com Freio Motor (Short Brake)
        gpio_set_level(motor->pin_in1, 1);
        gpio_set_level(motor->pin_in2, 1);
        ledc_set_duty(MOTOR_PWM_MODE, motor->pwm_channel, 0);
    }

    ledc_update_duty(MOTOR_PWM_MODE, motor->pwm_channel);
}