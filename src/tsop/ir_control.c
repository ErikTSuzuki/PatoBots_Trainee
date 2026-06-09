/**
 * ir_control.c
 *
 * Decodificador NEC via periférico RMT do ESP32 (ESP-IDF v5.x).
 * Integrado ao projeto sumô — usa as mesmas variáveis globais
 * modo_atual, estrategia_atual e startup_fase de robot_task.c.
 *
 * Protocolo NEC:
 *   Leader: 9000µs HIGH + 4500µs LOW
 *   Bit '1': 560µs HIGH + 1690µs LOW
 *   Bit '0': 560µs HIGH + 560µs LOW
 *   Repeat : 9000µs HIGH + 2250µs LOW + 560µs HIGH
 *
 * O TSOP1838/VS1838B inverte o sinal fisicamente.
 * O RMT é configurado com .flags.invert_in = true para compensar.
 */

#include "ir_control.h"
#include "robot_states.h"
#include "motor.h"
#include "robot_task.h"
#include "config.h"

#include "driver/rmt_rx.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "IR_CTRL";

// ─────────────────────────────────────────────
//  Tolerâncias NEC (µs)
// ─────────────────────────────────────────────
#define NEC_LEADER_HI   9000
#define NEC_LEADER_LO   4500
#define NEC_REPEAT_LO   2250
#define NEC_BIT_HI       560
#define NEC_ONE_LO      1690
#define NEC_ZERO_LO      560
#define NEC_TOL          220

#define IN_RANGE(v, ref, tol) \
    ((int32_t)(v) > (int32_t)((ref)-(tol)) && \
     (int32_t)(v) < (int32_t)((ref)+(tol)))

// ─────────────────────────────────────────────
//  Estado interno do módulo
// ─────────────────────────────────────────────
static rmt_channel_handle_t   rx_chan   = NULL;
static QueueHandle_t          ir_queue  = NULL;
static rmt_symbol_word_t      rx_buf[64];
static rmt_receive_config_t   rx_cfg;

// Controle do startup (contagem regressiva 5s)
static int64_t   startup_inicio_ms   = 0;
static bool      ir_rc_mode_ativo    = false;

// Último comando recebido (para modo RC com repetição de frame)
static uint8_t   ultimo_cmd          = 0xFF;
static int64_t   ultimo_cmd_ms       = 0;

// ─────────────────────────────────────────────
//  Callback RMT — chamado na ISR, envia para queue
// ─────────────────────────────────────────────
static bool IRAM_ATTR rmt_rx_done_cb(rmt_channel_handle_t ch,
                                      const rmt_rx_done_event_data_t *ev,
                                      void *user_ctx)
{
    BaseType_t high_task = pdFALSE;
    xQueueSendFromISR(ir_queue, ev, &high_task);
    return high_task == pdTRUE;
}

// ─────────────────────────────────────────────
//  Decodificador NEC
// ─────────────────────────────────────────────
typedef struct {
    uint8_t command;
    bool    repeat;
    bool    valid;
} nec_result_t;

static bool nec_decode(const rmt_symbol_word_t *s, size_t n, nec_result_t *out)
{
    if (n < 2) return false;

    uint32_t hi0 = s[0].duration0;
    uint32_t lo0 = s[0].duration1;

    // Frame de repetição (botão mantido pressionado)
    if (IN_RANGE(hi0, NEC_LEADER_HI, 500) && IN_RANGE(lo0, NEC_REPEAT_LO, 300)) {
        out->repeat  = true;
        out->command = ultimo_cmd;
        out->valid   = true;
        return true;
    }

    // Leader do frame normal
    if (!IN_RANGE(hi0, NEC_LEADER_HI, 500)) return false;
    if (!IN_RANGE(lo0, NEC_LEADER_LO, 500)) return false;
    if (n < 34) return false;

    uint32_t data = 0;
    for (int i = 0; i < 32; i++) {
        if (!IN_RANGE(s[1+i].duration0, NEC_BIT_HI, NEC_TOL)) return false;

        if      (IN_RANGE(s[1+i].duration1, NEC_ONE_LO,  NEC_TOL)) data |= (1u << i);
        else if (!IN_RANGE(s[1+i].duration1, NEC_ZERO_LO, NEC_TOL)) return false;
    }

    // Verifica integridade: cada byte deve ser complemento do seguinte
    uint8_t addr     = (data >>  0) & 0xFF;
    uint8_t addr_inv = (data >>  8) & 0xFF;
    uint8_t cmd      = (data >> 16) & 0xFF;
    uint8_t cmd_inv  = (data >> 24) & 0xFF;

    if ((uint8_t)(addr ^ addr_inv) != 0xFF) return false;
    if ((uint8_t)(cmd  ^ cmd_inv ) != 0xFF) return false;

    out->command = cmd;
    out->repeat  = false;
    out->valid   = true;
    return true;
}

// ─────────────────────────────────────────────
//  Helpers de motor (acesso direto via robot_task)
// ─────────────────────────────────────────────
static inline void parar_motores(void)
{
    motor_set_speed(get_motor_direito(),  0);
    motor_set_speed(get_motor_esquerdo(), 0);
}

// ─────────────────────────────────────────────
//  Processamento de comando IR
// ─────────────────────────────────────────────
static void processar_comando(uint8_t cmd, bool repeat)
{
    // ── Emergência global (qualquer estado) ──────────────────────────────
    if (cmd == IR_BTN_STAR || cmd == IR_BTN_HASH) {
        parar_motores();
        modo_atual       = MODO_DESATIVADO;
        startup_fase     = STARTUP_IDLE;
        ir_rc_mode_ativo = false;
        ESP_LOGI(TAG, "EMERGÊNCIA — robô desativado");
        return;
    }

    // Frames de repetição só interessam no modo RC IR (botão mantido)
    if (repeat) {
        if (!ir_rc_mode_ativo) return;
        // Cai no switch abaixo com o último comando
        cmd = ultimo_cmd;
    }

    switch (cmd) {

        // ── [1] Iniciar com contagem regressiva 5s ────────────────────────
        case IR_BTN_1:
            if (startup_fase == STARTUP_COUNTING) {
                // Já contando, ignora pressionamento duplo
                break;
            }
            // Se nunca configurou modo, usa FRENTE_TOTAL como padrão
            if (startup_fase == STARTUP_IDLE && modo_atual == MODO_DESATIVADO) {
                estrategia_atual = ESTRATEGIA_FRENTE_TOTAL;
            }
            parar_motores();
            modo_atual       = MODO_DESATIVADO;
            ir_rc_mode_ativo = false;
            startup_fase     = STARTUP_COUNTING;
            startup_inicio_ms = esp_timer_get_time() / 1000;
            ESP_LOGI(TAG, "[BTN1] Contagem iniciada — %dms até ativar", STARTUP_DELAY_MS);
            break;

        // ── [2] Configura estratégia: avançar direto ──────────────────────
        case IR_BTN_2:
            estrategia_atual = ESTRATEGIA_FRENTE_TOTAL;
            startup_fase     = STARTUP_IDLE;
            ESP_LOGI(TAG, "[BTN2] Estratégia: FRENTE_TOTAL configurada");
            // Não inicia — aguarda BTN1
            break;

        // ── [3] Configura estratégia: percorrer borda ─────────────────────
        case IR_BTN_3:
            estrategia_atual = ESTRATEGIA_PERCORRER_BORDA;
            startup_fase     = STARTUP_IDLE;
            ESP_LOGI(TAG, "[BTN3] Estratégia: PERCORRER_BORDA configurada");
            // Não inicia — aguarda BTN1
            break;

        // ── [4] Modo RC via IR (imediato, sem delay) ──────────────────────
        case IR_BTN_4:
            parar_motores();
            modo_atual        = MODO_RC;
            ir_rc_mode_ativo  = true;
            startup_fase      = STARTUP_IDLE;
            ESP_LOGI(TAG, "[BTN4] Modo RC IR ativado");
            break;

        // ── Direcionais RC IR ─────────────────────────────────────────────
        case IR_BTN_UP:
            if (ir_rc_mode_ativo) {
                motor_set_speed(get_motor_direito(),   IR_RC_VEL_FRENTE);
                motor_set_speed(get_motor_esquerdo(),  IR_RC_VEL_FRENTE);
                ultimo_cmd_ms = esp_timer_get_time() / 1000;
            }
            break;

        case IR_BTN_DOWN:
            if (ir_rc_mode_ativo) {
                motor_set_speed(get_motor_direito(),  -IR_RC_VEL_RE);
                motor_set_speed(get_motor_esquerdo(), -IR_RC_VEL_RE);
                ultimo_cmd_ms = esp_timer_get_time() / 1000;
            }
            break;

        case IR_BTN_LEFT:
            if (ir_rc_mode_ativo) {
                motor_set_speed(get_motor_direito(),   IR_RC_VEL_GIRO);
                motor_set_speed(get_motor_esquerdo(), -IR_RC_VEL_GIRO);
                ultimo_cmd_ms = esp_timer_get_time() / 1000;
            }
            break;

        case IR_BTN_RIGHT:
            if (ir_rc_mode_ativo) {
                motor_set_speed(get_motor_direito(),  -IR_RC_VEL_GIRO);
                motor_set_speed(get_motor_esquerdo(),  IR_RC_VEL_GIRO);
                ultimo_cmd_ms = esp_timer_get_time() / 1000;
            }
            break;

        case IR_BTN_OK:
            if (ir_rc_mode_ativo) {
                parar_motores();
                ultimo_cmd_ms = esp_timer_get_time() / 1000;
            }
            break;

        default:
            ESP_LOGD(TAG, "Comando desconhecido: 0x%02X", cmd);
            break;
    }

    if (!repeat) ultimo_cmd = cmd;
}

// ─────────────────────────────────────────────
//  Task principal do módulo IR
// ─────────────────────────────────────────────
static void ir_task(void *arg)
{
    rmt_rx_done_event_data_t ev;

    // Inicia a primeira recepção
    rmt_receive(rx_chan, rx_buf, sizeof(rx_buf), &rx_cfg);

    while (1) {
        // 1. Processa frames IR recebidos
        if (xQueueReceive(ir_queue, &ev, pdMS_TO_TICKS(10)) == pdTRUE) {
            nec_result_t res = {0};
            if (nec_decode(ev.received_symbols, ev.num_symbols, &res) && res.valid) {
                ESP_LOGI(TAG, "IR cmd=0x%02X repeat=%d", res.command, res.repeat);
                processar_comando(res.command, res.repeat);
            }
            // Reinicia recepção para o próximo frame
            rmt_receive(rx_chan, rx_buf, sizeof(rx_buf), &rx_cfg);
        }

        // 2. Lógica de contagem regressiva do startup (5s)
        if (startup_fase == STARTUP_COUNTING) {
            int64_t agora = esp_timer_get_time() / 1000;
            int64_t decorrido = agora - startup_inicio_ms;

            if (decorrido >= STARTUP_DELAY_MS) {
                // Contagem concluída — ativa o modo autônomo
                modo_atual   = MODO_AUTONOMO;
                startup_fase = STARTUP_DONE;
                ESP_LOGI(TAG, "Startup concluído! Modo AUTONOMO ativo. Estratégia: %d",
                         estrategia_atual);
            } else {
                // Log periódico de contagem (a cada 1s)
                static int64_t ultimo_log = 0;
                if (agora - ultimo_log >= 1000) {
                    ultimo_log = agora;
                    ESP_LOGI(TAG, "Iniciando em %llds...",
                             (long long)((STARTUP_DELAY_MS - decorrido) / 1000 + 1));
                }
            }
        }

        // 3. Segurança RC IR: para se não receber comando por IR_RC_TIMEOUT_MS
        if (ir_rc_mode_ativo && modo_atual == MODO_RC) {
            int64_t agora = esp_timer_get_time() / 1000;
            if (ultimo_cmd_ms > 0 && (agora - ultimo_cmd_ms) > IR_RC_TIMEOUT_MS) {
                parar_motores();
                // Não zera ultimo_cmd_ms aqui, só para — aguarda próximo botão
            }
        }
    }
}

// ─────────────────────────────────────────────
//  API pública
// ─────────────────────────────────────────────

void ir_control_init(void)
{
    ir_queue = xQueueCreate(4, sizeof(rmt_rx_done_event_data_t));

    // Configura canal RMT de recepção
    rmt_rx_channel_config_t chan_cfg = {
        .gpio_num          = PIN_IR_TSOP,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 1000000,    // 1 tick = 1µs
        .mem_block_symbols = 64,
        .flags = {
            .invert_in = true,           // TSOP inverte o sinal — compensa aqui
        },
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&chan_cfg, &rx_chan));

    // Registra callback de recepção
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rmt_rx_done_cb };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_chan, &cbs, NULL));
    ESP_ERROR_CHECK(rmt_enable(rx_chan));

    // Parâmetros de recepção
    rx_cfg.signal_range_min_ns = 1250;       // filtra glitches < 1.25µs
    rx_cfg.signal_range_max_ns = 12000000;   // timeout de frame = 12ms

    // Task leve — stack 3KB, prioridade 4 (abaixo do robot_task que é 5)
    xTaskCreate(ir_task, "ir_task", 3072, NULL, 4, NULL);

    ESP_LOGI(TAG, "TSOP IR iniciado no GPIO %d", PIN_IR_TSOP);
}

bool ir_rc_ativo(void)
{
    return ir_rc_mode_ativo;
}