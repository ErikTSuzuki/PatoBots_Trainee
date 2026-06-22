#ifndef IR_CONTROL_H_
#define IR_CONTROL_H_

/**
 * ir_control.h
 *
 * Receptor TSOP via GPIO + interrupção.
 * Sem dependência de rmt_rx.h — funciona no ESP-IDF v4.x e v5.x.
 *
 * Responsabilidade: apenas ativar o robô e selecionar estratégia.
 * O controle do Xbox (Bluepad32) continua sendo o RC.
 *
 * Mapeamento:
 *   [1]  → Iniciar: 5s de delay e ativa a estratégia configurada
 *   [2]  → Configura ESTRATEGIA_FRENTE_TOTAL  (sem iniciar)
 *   [3]  → Configura ESTRATEGIA_PERCORRER_BORDA (sem iniciar)
 *   [*]  → Emergência: desativa robô de qualquer estado
 */

void ir_control_init(void);

#endif /* IR_CONTROL_H_ */