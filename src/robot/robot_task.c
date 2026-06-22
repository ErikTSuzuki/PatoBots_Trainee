#include "robot_task.h"
<<<<<<< HEAD
=======
#include "hal/adc_types.h"
>>>>>>> upstream/main
#include "robot_states.h"
#include "motor.h"
#include "sensor_distancia.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

<<<<<<< HEAD
ModoRobo modo_atual = MODO_RC;
=======
ModoRobo modo_atual = MODO_DESATIVADO;
EstrategiaAutonoma estrategia_atual = ESTRATEGIA_FRENTE_TOTAL;
StartupFase startup_fase = STARTUP_IDLE;
>>>>>>> upstream/main

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
<<<<<<< HEAD
    modo_atual = MODO_RC;
=======
    modo_atual = MODO_DESATIVADO;
	estrategia_atual = ESTRATEGIA_FRENTE_TOTAL;
	startup_fase = STARTUP_IDLE;
>>>>>>> upstream/main
}

void robot_logic_task(void *pvParameters) {
    printf("[Robot] Tarefa de combate iniciada!\n");

    while (1) {
<<<<<<< HEAD
=======
		// Se o robô estiver desativado, garante motores parados
		if (modo_atual == MODO_DESATIVADO || startup_fase == STARTUP_COUNTING) {
            motor_set_speed(get_motor_direito(), 0);
	        motor_set_speed(get_motor_esquerdo(), 0);
       		vTaskDelay(pdMS_TO_TICKS(50));
            continue;
		}
		// 1. LÓGICA DE ESCAPE DA LINHA BRANCA (CRÍTICA)
		
		int linha_esquerda = ler_sensor_raw(ADC_CHANNEL_4); //QTR ESQ
		int linha_direita = ler_sensor_raw(ADC_CHANNEL_5); //QTR DIR
		
		const int limite_linha_branca = 2000;
		
		if (linha_esquerda > limite_linha_branca || linha_direita > limite_linha_branca){
			//se achar qualquer linha da re
			motor_set_speed(get_motor_direito(), -800);
			motor_set_speed(get_motor_esquerdo(), -800);
			vTaskDelay(pdMS_TO_TICKS(300)); //300ms de re
			
			//gira para a esquerda
			if (linha_esquerda > limite_linha_branca){
				motor_set_speed(get_motor_esquerdo(), 700);
				motor_set_speed(get_motor_direito(), -700);
			}
			//gira para a diretia
			else{
				motor_set_speed(get_motor_direito(), 700);
				motor_set_speed(get_motor_esquerdo(), -700);
			}
			vTaskDelay(pdMS_TO_TICKS(200));
			continue;
		}
		
		//2. EXECUÇÃO DOS MODOS
		
>>>>>>> upstream/main
        if (modo_atual == MODO_AUTONOMO) {
            // Lê sensor frontal IR
            int raw = ler_sensor_raw(ADC_CHANNEL_7);
            int cm  = raw_para_cm(raw);

            // Lógica de combate: se inimigo próximo, ataca!
            if (cm < DISTANCIA_ATAQUE_CM && cm != 999) {
                motor_set_speed(&motor_direito, 1023);
                motor_set_speed(&motor_esquerdo, 1023);
            } else {
<<<<<<< HEAD
                // Comportamento alternativo: girar para procurar
                motor_set_speed(&motor_direito, 500);
                motor_set_speed(&motor_esquerdo, -500);
=======
				switch (estrategia_atual) {
				                 
					case ESTRATEGIA_FRENTE_TOTAL:
		            // Avança procurando
                    motor_set_speed(get_motor_direito(), 600);
				    motor_set_speed(get_motor_esquerdo(), 600);
                    break;

		           case ESTRATEGIA_PERCORRER_BORDA:
	               // Faz uma curva aberta contornando o ringue
				   // Um motor mais rápido que o outro faz ele andar em arco
	               motor_set_speed(get_motor_direito(), 400);
	               motor_set_speed(get_motor_esquerdo(), 800);
	               break;

				   default:
				   motor_set_speed(get_motor_direito(), 0);
				   motor_set_speed(get_motor_esquerdo(), 0);
				   break;
				}
>>>>>>> upstream/main
            }
        }
        // Delay para o Watchdog do ESP32 não reclamar
        vTaskDelay(pdMS_TO_TICKS(20));
    }
<<<<<<< HEAD
}
=======
}
>>>>>>> upstream/main
