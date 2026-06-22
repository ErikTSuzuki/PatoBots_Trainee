#ifndef BT_CONTROL_H_
#define BT_CONTROL_H_

#include "motor.h"

/**
 * @brief Obtém o motor direito para uso externo
 */
motor_config_t* get_motor_direito(void);

/**
 * @brief Obtém o motor esquerdo para uso externo
 */
motor_config_t* get_motor_esquerdo(void);

/**
 * @brief Função callback necessária para o Bluepad32
 * Retorna a plataforma customizada
 */
struct uni_platform* get_my_platform(void);

#endif /* BT_CONTROL_H_ */
#ifndef BT_CONTROL_H_
#define BT_CONTROL_H_

#include "motor.h"

/**
 * @brief Obtém o motor direito para uso externo
 */
motor_config_t* get_motor_direito(void);

/**
 * @brief Obtém o motor esquerdo para uso externo
 */
motor_config_t* get_motor_esquerdo(void);

/**
 * @brief Função callback necessária para o Bluepad32
 * Retorna a plataforma customizada
 */
struct uni_platform* get_my_platform(void);

#endif /* BT_CONTROL_H_ */
