/*
 * robot_task.c
 * ----------------------------------------------------------------------------
 * Port do automato "testeSumo" (gerado para MSP430Gxx, saida Moore, jogador de
 * automato em listas encadeadas, por Cesar Rafael Claure Torrico) para o
 * projeto PatoBots (ESP32 / ESP-IDF / FreeRTOS).
 *
 * A ESTRUTURA do testeSumo.c foi mantida igual: tabelas de eventos/estados,
 * buffer de eventos, gerador de eventos controlaveis (switch(g)), jogador de
 * automato e o switch(current_state) da saida Moore. So foi trocado o que o
 * hardware exige:
 *   - Portas P2 (saidas) -> motor_set_speed() (ponte H do projeto).
 *   - Interrupcao RTI_PORT1 (pinos P1 digitais) -> task "rotina_sensores",
 *     pois os sensores deste projeto sao ANALOGICOS (ADC) e nao podem ser
 *     lidos dentro de uma ISR. O corpo segue a mesma forma do RTI_PORT1.
 *   - Timer A (TACTL/TAIFG/tempo_ms) -> temporizacao por tick do FreeRTOS.
 *
 * NOVO (unico conteudo criado): as funcoes de acao de cada estado
 * (acao_atacar, acao_espera, ...), chamadas dentro do switch(current_state).
 * ----------------------------------------------------------------------------
 */

#include "robot_task.h"
#include "robot_states.h"
#include "motor.h"
#include "sensor_distancia.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdbool.h>

// Dados do automato (igual ao testeSumo.c)
#define NTRANS   16   // Numero de Transicoes
#define NESTADOS 9    // Numero de Estados
#define BUFFER   10   // Maximo Numero de Eventos no Buffer

const unsigned int event[NTRANS]    = {1,3,6,7,8,0,8,2,9,9,2,4,1,0,5,3};
const unsigned int in_state[NTRANS] = {8,7,3,1,4,7,3,8,3,4,0,5,3,0,6,4};
const unsigned int rfirst[NESTADOS] = {2,3,4,6,8,9,10,13,16};
const unsigned int rnext[NTRANS]    = {0,1,0,0,0,5,0,7,0,0,0,11,12,0,14,15};

// Mapeamento de eventos nao controlaveis como entradas (igual ao testeSumo.c)
#define SF_Dir_achou  0   // Entrada 0
#define SF_Dir_perdeu 1   // Entrada 1
#define SF_Esq_achou  2   // Entrada 2
#define SF_Esq_perdeu 3   // Entrada 3
#define SL_Dir_achou  4   // Entrada 4
#define SL_Esq_achou  5   // Entrada 5

// Mapeamento de eventos controlaveis (igual ao testeSumo.c)
#define iniciar       6
#define prepara       7
#define tempoProcura  8
#define tempoRe       9

/* ===========================================================================
 * ADAPTACAO DE HARDWARE (ESP32)
 * No MSP a saida era P2OUT (8 saidas digitais). Aqui a "saida" do automato
 * sao os 2 motores. Os parametros abaixo (limiares e velocidades) sao
 * tunaveis e substituem o mapeamento direto de pinos do testeSumo.c.
 * ===========================================================================*/
#define LIMIAR_DETECCAO_CM   28    // IR frontal: inimigo "achado" se cm < isto
#define LIMIAR_LINHA_RAW     2000  // QTR: linha branca se raw > isto

#define VEL_ATAQUE           1023  // frente total
#define VEL_PROCURA          650   // giro de varredura
#define VEL_VIRA             600   // giro de alinhamento
#define VEL_RE               800   // ré

#define PERIODO_EVENTO_MS    150   // periodo do "Timer A" (tempo_ms) p/ eventos
#define PERIODO_LOOP_MS      20    // periodo do laco principal (watchdog)
#define PERIODO_SENSOR_MS    20    // periodo da rotina de sensores ("ISR")

// Declaracao de variaveis globais (igual ao testeSumo.c)
unsigned int buffer[BUFFER];   // Buffer para a fila de eventos externos
unsigned int n_buffer = 0;     // Numero de eventos no Buffer

// Protege o buffer entre a task de sensores e o laco principal (no MSP isso
// era garantido por estar dentro da ISR; aqui usamos secao critica).
static portMUX_TYPE buf_mux = portMUX_INITIALIZER_UNLOCKED;

/* ===========================================================================
 * MOTORES e ESTADO GLOBAL do robo (mantidos do projeto original)
 * ===========================================================================*/
static motor_config_t motor_direito = {
    .pin_in1 = PIN_MOTOR_RIGHT_IN1, 
    .pin_in2 = PIN_MOTOR_RIGHT_IN2,
    .pin_pwm = PIN_MOTOR_RIGHT_PWM, 
    .pwm_channel = MOTOR_RIGHT_CANAL
};
static motor_config_t motor_esquerdo = {
    .pin_in1 = PIN_MOTOR_LEFT_IN1,  
    .pin_in2 = PIN_MOTOR_LEFT_IN2,
    .pin_pwm = PIN_MOTOR_LEFT_PWM,  
    .pwm_channel = MOTOR_LEFT_CANAL
};

motor_config_t* get_motor_direito(void)  { return &motor_direito; }
motor_config_t* get_motor_esquerdo(void) { return &motor_esquerdo; }

ModoRobo           modo_atual       = MODO_DESATIVADO;
EstrategiaAutonoma estrategia_atual = ESTRATEGIA_FRENTE_TOTAL;
StartupFase        startup_fase     = STARTUP_IDLE;

/* ===========================================================================
 * "Timer A" do MSP adaptado ao tick do FreeRTOS (substitui TACTL/TAIFG/tempo_ms)
 * ===========================================================================*/
static volatile uint32_t timer_deadline = 0;

static inline uint32_t millis(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}
// Equivalente ao tempo_ms() do testeSumo.c: arma o estouro do "timer".
static void tempo_ms(unsigned int tempo) {
    timer_deadline = millis() + tempo;
}
// Equivalente a (TACTL & TAIFG): 1 quando o "timer" estoura.
static int timer_estourou(void) {
    return ((int32_t)(millis() - timer_deadline) >= 0);
}

/* ===========================================================================
 * SAIDA MOORE — FUNCOES DE ACAO DE CADA ESTADO  (UNICO CONTEUDO NOVO)
 * No testeSumo.c estes eram os "case" vazios com // Adicionar Acao...
 * Convencao: speed > 0 = roda para frente.
 * Giro para a DIREITA (horario): roda esquerda p/ frente, direita p/ tras.
 * Se o seu robo girar ao contrario, basta inverter os sinais aqui.
 * ===========================================================================*/
static void parar_motores(void) {
    motor_set_speed(get_motor_direito(),  0);
    motor_set_speed(get_motor_esquerdo(), 0);
}

static void acao_atacar(void) {        // Estado 0: atacar
    motor_set_speed(get_motor_esquerdo(), VEL_ATAQUE);
    motor_set_speed(get_motor_direito(),  VEL_ATAQUE);
}
static void acao_espera(void) {        // Estado 1: espera
    parar_motores();
}
static void acao_inicio(void) {        // Estado 2: inicio
    parar_motores();
}
static void acao_procuraDir(void) {    // Estado 3: procuraDir (varre p/ direita)
    motor_set_speed(get_motor_esquerdo(),  VEL_PROCURA);
    motor_set_speed(get_motor_direito(),  -VEL_PROCURA);
}
static void acao_procuraEsq(void) {    // Estado 4: procuraEsq (varre p/ esquerda)
    motor_set_speed(get_motor_esquerdo(), -VEL_PROCURA);
    motor_set_speed(get_motor_direito(),   VEL_PROCURA);
}
static void acao_reDir(void) {         // Estado 5: reDir (linha a direita -> ré)
    motor_set_speed(get_motor_esquerdo(), -VEL_RE);
    motor_set_speed(get_motor_direito(),  -(VEL_RE * 3) / 5); // leve curva
}
static void acao_reEsq(void) {         // Estado 6: reEsq (linha a esquerda -> ré)
    motor_set_speed(get_motor_esquerdo(), -(VEL_RE * 3) / 5); // leve curva
    motor_set_speed(get_motor_direito(),  -VEL_RE);
}
static void acao_viraDir(void) {       // Estado 7: viraDir (alinha p/ direita)
    motor_set_speed(get_motor_esquerdo(),  VEL_VIRA);
    motor_set_speed(get_motor_direito(),  -VEL_VIRA);
}
static void acao_viraEsq(void) {       // Estado 8: viraEsq (alinha p/ esquerda)
    motor_set_speed(get_motor_esquerdo(), -VEL_VIRA);
    motor_set_speed(get_motor_direito(),   VEL_VIRA);
}

/* ===========================================================================
 * "INTERRUPCAO" DOS SENSORES — equivalente ao RTI_PORT1 do testeSumo.c.
 * No MSP cada pino de P1 (borda) empilhava 1 evento. Aqui, como os sensores
 * sao analogicos, lemos o ADC e empilhamos os mesmos eventos por BORDA, com
 * o mesmo padrao do original: buffer[n_buffer]=EVENTO; if(n_buffer<BUFFER-1)..
 * Roda em uma task dedicada (ADC nao pode ser lido dentro de ISR no ESP).
 * ===========================================================================*/
static void rotina_sensores(void *arg) {
    bool sf_dir_ant = false, sf_esq_ant = false;
    bool sl_dir_ant = false, sl_esq_ant = false;

    while (1) {
        // So gera eventos em combate (evita encher o buffer parado/em RC)
        if (modo_atual != MODO_AUTONOMO || startup_fase == STARTUP_COUNTING) {
            sf_dir_ant = sf_esq_ant = sl_dir_ant = sl_esq_ant = false;
            vTaskDelay(pdMS_TO_TICKS(PERIODO_SENSOR_MS));
            continue;
        }

        // Leitura dos sensores do projeto
        int cm_dir = raw_para_cm(ler_sensor_raw(ADC_CHANNEL_7)); // IR Direita
        int cm_esq = raw_para_cm(ler_sensor_raw(ADC_CHANNEL_0)); // IR Esquerda
        int lin_dir = ler_sensor_raw(ADC_CHANNEL_5);             // QTR Direita
        int lin_esq = ler_sensor_raw(ADC_CHANNEL_4);             // QTR Esquerda

        bool sf_dir = (cm_dir != 999) && (cm_dir < LIMIAR_DETECCAO_CM);
        bool sf_esq = (cm_esq != 999) && (cm_esq < LIMIAR_DETECCAO_CM);
        bool sl_dir = (lin_dir > LIMIAR_LINHA_RAW);
        bool sl_esq = (lin_esq > LIMIAR_LINHA_RAW);

        portENTER_CRITICAL(&buf_mux);
        // SF Direita: achou / perdeu
        if (sf_dir && !sf_dir_ant) { buffer[n_buffer] = SF_Dir_achou;  if (n_buffer < BUFFER-1) n_buffer++; }
        if (!sf_dir && sf_dir_ant) { buffer[n_buffer] = SF_Dir_perdeu; if (n_buffer < BUFFER-1) n_buffer++; }
        // SF Esquerda: achou / perdeu
        if (sf_esq && !sf_esq_ant) { buffer[n_buffer] = SF_Esq_achou;  if (n_buffer < BUFFER-1) n_buffer++; }
        if (!sf_esq && sf_esq_ant) { buffer[n_buffer] = SF_Esq_perdeu; if (n_buffer < BUFFER-1) n_buffer++; }
        // SL Direita / Esquerda: apenas "achou" (borda de subida) — igual ao automato
        if (sl_dir && !sl_dir_ant) { buffer[n_buffer] = SL_Dir_achou;  if (n_buffer < BUFFER-1) n_buffer++; }
        if (sl_esq && !sl_esq_ant) { buffer[n_buffer] = SL_Esq_achou;  if (n_buffer < BUFFER-1) n_buffer++; }
        portEXIT_CRITICAL(&buf_mux);

        sf_dir_ant = sf_dir; sf_esq_ant = sf_esq;
        sl_dir_ant = sl_dir; sl_esq_ant = sl_esq;

        vTaskDelay(pdMS_TO_TICKS(PERIODO_SENSOR_MS));
    }
}

/* ===========================================================================
 * INICIALIZACAO
 * No MSP o main() chamava config_clk/timer/io. Aqui isso ja e feito no
 * app_main() (motor_init, inicializar_sensores). So criamos a "ISR" de
 * sensores e armamos o timer.
 * ===========================================================================*/
void robot_init(void) {
    modo_atual       = MODO_DESATIVADO;
    estrategia_atual = ESTRATEGIA_FRENTE_TOTAL;
    startup_fase     = STARTUP_IDLE;
    n_buffer         = 0;
    tempo_ms(PERIODO_EVENTO_MS);
    xTaskCreate(rotina_sensores, "sensores", 3072, NULL, 6, NULL);
}

/* ===========================================================================
 * LACO PRINCIPAL — corpo do main() do testeSumo.c (estrutura preservada)
 * ===========================================================================*/
void robot_logic_task(void *pvParameters) {
    printf("[Robot] Automato testeSumo iniciado (port ESP32)!\n");

    unsigned int k;
    int occur_event;                   // Evento ocorrido
    unsigned int current_state = 2;    // Estado atual inicializado com inicial
    char g = 0;                        // Flag para gerador aleatorio de eventos
    char gerar_evento = 1;             // Habilita temporizacao de controlaveis
    char moore_output = 0;             // Inicializa saida periferica

    while (1) {
        // --- Gate de modo (ESP): so executa a FSM em MODO_AUTONOMO. Em RC quem
        //     comanda e o controle (my_platform.c); desativado -> motores 0. ---
        if (modo_atual != MODO_AUTONOMO || startup_fase == STARTUP_COUNTING) {
            if (modo_atual != MODO_RC) parar_motores();
            // reinicia o automato para um proximo combate
            current_state = 2; g = 0; gerar_evento = 1; moore_output = 0;
            portENTER_CRITICAL(&buf_mux); n_buffer = 0; portEXIT_CRITICAL(&buf_mux);
            tempo_ms(PERIODO_EVENTO_MS);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        occur_event = -1;

        if (n_buffer == 0) { // sem evento no buffer -> gerar evento controlavel
            if (timer_estourou()) { // Se o "timer" estourar, habilita a geracao
                gerar_evento = 1;
            }
            if (gerar_evento == 1) {
                switch (g) { // gerador automatico de eventos controlaveis
                    case (0):
                        occur_event = iniciar;
                        g++;
                        break;
                    case (1):
                        occur_event = prepara;
                        g++;
                        break;
                    case (2):
                        occur_event = tempoProcura;
                        g++;
                        break;
                    case (3):
                        occur_event = tempoRe;
                        g = 0;
                        break;
                }
                gerar_evento = 0;            // consome este disparo
                tempo_ms(PERIODO_EVENTO_MS); // rearma o "timer"
            }
        }
        else { // se existir evento nao controlavel, pegar do buffer
            portENTER_CRITICAL(&buf_mux);
            occur_event = buffer[0];
            n_buffer--;
            k = 0;
            while (k < n_buffer) {
                buffer[k] = buffer[k + 1];
                k += 1;
            }
            portEXIT_CRITICAL(&buf_mux);
        }

        // Jogador de automato (identico ao testeSumo.c)
        if (occur_event >= 0) {
            k = rfirst[current_state];
            if (k == 0) {
                parar_motores(); // Dead Lock!!! (no MSP era return;)
            }
            else {
                while (k > 0) {
                    k -= 1;
                    if (event[k] == (unsigned int)occur_event) {
                        current_state = in_state[k];
                        moore_output = 1;
                        break;
                    }
                    k = rnext[k];
                }
            }
        }

        if (moore_output) { // evento valido -> imprime saida fisica (Moore)
            gerar_evento = 1;
            switch (current_state) {
                case (0): acao_atacar();    break; // atacar
                case (1): acao_espera();    break; // espera
                case (2): acao_inicio();    break; // inicio
                case (3): acao_procuraDir(); break; // procuraDir
                case (4): acao_procuraEsq(); break; // procuraEsq
                case (5): acao_reDir();     break; // reDir
                case (6): acao_reEsq();     break; // reEsq
                case (7): acao_viraDir();   break; // viraDir
                case (8): acao_viraEsq();   break; // viraEsq
            } // fim switch
            moore_output = 0;
            occur_event = -1;
        } // fim if(moore_output)

        vTaskDelay(pdMS_TO_TICKS(PERIODO_LOOP_MS)); // alimenta o watchdog
    } // fim while(1)
} // fim robot_logic_task