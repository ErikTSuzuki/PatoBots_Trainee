#include "sensor_distancia.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "../utils/config.h"

static adc_oneshot_unit_handle_t adc1_handle;

void inicializar_sensores(void) {
    // 1. Configura a unidade ADC1
    adc_oneshot_unit_init_cfg_t cfg_unidade = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&cfg_unidade, &adc1_handle);

    // 2. Configuração padrão dos canais (12dB = lê até ~3.1V)
    adc_oneshot_chan_cfg_t cfg_canal = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    // 3. Aplica a configuração em cada canal
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &cfg_canal); // IR Frente  - GPIO34
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &cfg_canal); // IR Direita - GPIO35
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &cfg_canal); // IR Esquerda- GPIO36
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &cfg_canal); // QTR Esq   - GPIO32
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &cfg_canal); // QTR Dir   - GPIO33
}

int ler_sensor_raw(adc_channel_t canal) {
    int soma = 0;
    for (int i = 0; i < 8; i++) { // le 8 amostras
        int valor = 0;
        adc_oneshot_read(adc1_handle, canal, &valor);
        soma += valor;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return soma / 8;//faz a média dos valores
}

int raw_para_cm(int raw) {
    if (raw < 100) return 999;
    float tensao = raw * (3.1f / 4095.0f);
    float cm = 13.0f * pow(tensao, -1.0f);

    if (cm < DISTANCIA_MIN_CM)   cm = DISTANCIA_MIN_CM;
    if (cm > DISTANCIA_MAX_CM) cm = DISTANCIA_MAX_CM;

    return (int)cm;
}
