#ifndef CONFIG_H_
#define CONFIG_H_

#include "driver/ledc.h"

// ============================================================================
// CONFIGURAÇÕES GERAIS DO PWM
// ============================================================================
#define MOTOR_PWM_FREQ       5000
#define MOTOR_PWM_RES        LEDC_TIMER_10_BIT
#define MOTOR_PWM_TIMER      LEDC_TIMER_0
#define MOTOR_PWM_MODE       LEDC_LOW_SPEED_MODE

// ============================================================================
// CONFIGURAÇÕES DE PINOS - MOTOR DIREITO
// ============================================================================
#define PIN_MOTOR_RIGHT_PWM     18
#define PIN_MOTOR_RIGHT_IN1     19
#define PIN_MOTOR_RIGHT_IN2     21
#define MOTOR_RIGHT_CANAL       LEDC_CHANNEL_0

// ============================================================================
// CONFIGURAÇÕES DE PINOS - MOTOR ESQUERDO
// ============================================================================
#define PIN_MOTOR_LEFT_PWM      22
#define PIN_MOTOR_LEFT_IN1      23
#define PIN_MOTOR_LEFT_IN2      25
#define MOTOR_LEFT_CANAL        LEDC_CHANNEL_1

// ============================================================================
// CONFIGURAÇÕES GERAIS
// ============================================================================
#define PIN_STBY                26

// ============================================================================
// CONFIGURAÇÕES DE SENSORES IR (DISTÂNCIA)
// ============================================================================
#define PIN_IR_FRENTE           34  // ADC1_CH6
#define PIN_IR_DIREITA          35  // ADC1_CH7
#define PIN_IR_ESQUERDA         36  // ADC1_CH0

// ============================================================================
// CONFIGURAÇÕES DE SENSORES QTR (LINHA)
// ============================================================================
#define PIN_QTR_ESQUERDA        32  // ADC1_CH4
#define PIN_QTR_DIREITA         33  // ADC1_CH5

// ============================================================================
// CONFIGURAÇÕES DE BOTÕES
// ============================================================================
#define PIN_BTN                 13

// ============================================================================
// CONFIGURAÇÕES DO TSOP (Sensor IR)
// ============================================================================
// IMPORTANTE: o TSOP inverte o sinal — o RMT já trata isso internamente.
#define PIN_IR_TSOP             27   // Livre nos pinos ADC/digitais do seu layout

// Botões do controle
#define IR_BTN_1        0x45    // [1] → Iniciar robô (dispara contagem 5s)
#define IR_BTN_2        0x46    // [2] → Estratégia: avançar direto
#define IR_BTN_3        0x47    // [3] → Estratégia: percorrer borda
#define IR_BTN_4        0x44    // [4] → Modo RC via controle

// ============================================================================
// CONSTANTES DE CONTROLE
// ============================================================================
#define ZONA_MORTA              40
#define DISTANCIA_ATAQUE_CM     25
#define DISTANCIA_MAX_CM        30
#define DISTANCIA_MIN_CM        4

// ============================================================================
// CONSTANTES DO STARTUP (contagem regressiva)
// ============================================================================
#define STARTUP_DELAY_MS        5000   // 5 segundos conforme regra oficial
#define STARTUP_LED_BLINK_MS    200    // LED piscando rápido durante contagem

// ============================================================================
// VELOCIDADES IRC (modo RC via controle IR)
// Valores em escala 0-1023 igual ao controle Xbox
// ============================================================================
#define IR_RC_VEL_FRENTE        800
#define IR_RC_VEL_RE            800
#define IR_RC_VEL_GIRO          650
#define IR_RC_TIMEOUT_MS        350    // Para se não receber comando por 350ms

#endif /* CONFIG_H_ */
