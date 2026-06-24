#ifndef ROBOT_TASK_H_
#define ROBOT_TASK_H_

#include <stdint.h>

/**
 * @brief Função task principal do robô
 * Roda continuamente e implementa a lógica de combate autônoma
 */
void robot_logic_task(void *pvParameters);

/**
 * @brief Inicializa o modo atual
 */
void robot_init(void);

#endif /* ROBOT_TASK_H_ */
